/*
 * mod-demonology-rework — Doombrand, the Grand Warlock's Design capstone (DESIGN_V2 §6).
 *
 * Doombrand (290014) brands an enemy with a fel sigil for 10s. While the brand (290504) is up,
 * every hit your demons land on the branded target charges a fraction (StorePct) of that damage
 * into the sigil — the accumulator lives in the demon-damage hook (ShardEconomy.cpp OnDamage),
 * clamped to CapSPCoef x owner spell power. When the brand ENDS the sigil detonates (290505):
 *   - EXPIRE (the 10s ran out): a single shadowflame hit on the target for the stored amount.
 *   - DEATH  (the target died): the stored amount is split evenly among enemies within
 *     Doombrand.AoeRadius of the corpse — the brand "bursts" through the pack.
 *   - CANCEL / DISPEL / EVADE: the sigil is purged with no detonation.
 * The detonation is cast BY THE WARLOCK (not a demon), so it never recurses into the demon-only
 * damage hook / Soul Harvest. The stored total is held on the owner's CommandPool (BrandStored).
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"

#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "Unit.h"

#include <list>

using namespace Demonology;

namespace
{
    constexpr uint32 TRIG = TRIGGERED_FULL_MASK;
}

// Doombrand (290014): spend a Soul Shard, stamp the brand debuff on the target, and arm the
// accumulator. A thin dummy shell — all the storing happens in the demon-damage hook.
class spell_demonology_doombrand : public SpellScript
{
    PrepareSpellScript(spell_demonology_doombrand);

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_FAILED_ERROR;
        Unit* target = GetExplTargetUnit();
        if (!target || !target->IsAlive() || caster->IsFriendlyTo(target))
            return SPELL_FAILED_BAD_TARGETS;
        if (caster->GetItemCount(ITEM_SOUL_SHARD) < gConfig.DoombrandShardCost)
            return SPELL_FAILED_REAGENTS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        if (gConfig.DoombrandShardCost)
            caster->DestroyItemCount(ITEM_SOUL_SHARD, gConfig.DoombrandShardCost, true);

        // Arm the accumulator FIRST, then apply the marker debuff (the hook needs both the
        // brand target set and HasAura(290504) to charge, so order doesn't matter — but this
        // keeps a stale store from a prior brand from ever being read).
        if (CommandPool* pool = sCommandPoolMgr->Find(caster->GetGUID()))
            pool->BrandApply(target->GetGUID());
        caster->CastSpell(target, SPELL_DOOMBRAND_DEBUFF, TRIG);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_doombrand::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_demonology_doombrand::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Doombrand debuff (290504): the visible brand. Its removal fires the detonation.
class spell_demonology_doombrand_debuff : public AuraScript
{
    PrepareAuraScript(spell_demonology_doombrand_debuff);

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* owner = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetUnitOwner();
        if (!owner || !target)
            return;

        CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID());
        if (!pool)
            return;
        // Guard: only fire for the brand this pool is tracking (the 20s CD prevents overlap,
        // but never detonate a store that belongs to a newer/other brand).
        if (pool->BrandTarget() != target->GetGUID())
            return;

        float const stored = pool->BrandStored();
        AuraRemoveMode const mode = GetTargetApplication()->GetRemoveMode();
        pool->BrandClear();

        if (stored <= 0.0f)
            return;

        if (mode == AURA_REMOVE_BY_EXPIRE)
        {
            // Single detonation on the still-living target.
            owner->CastCustomSpell(SPELL_DOOMBRAND_DETONATION, SPELLVALUE_BASE_POINT0, int32(stored), target, TRIG);
        }
        else if (mode == AURA_REMOVE_BY_DEATH)
        {
            // Death-detonation: split the stored damage among enemies around the corpse.
            std::list<Unit*> enemies;
            Acore::AnyUnfriendlyUnitInObjectRangeCheck check(target, owner, gConfig.DoombrandAoeRadius);
            Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, enemies, check);
            Cell::VisitObjects(target, searcher, gConfig.DoombrandAoeRadius);

            if (enemies.empty())
                return;                          // the pack is gone; the burst fizzles
            int32 const each = int32(stored / float(enemies.size()));
            if (each <= 0)
                return;
            for (Unit* e : enemies)
                if (e && e->IsAlive())
                    owner->CastCustomSpell(SPELL_DOOMBRAND_DETONATION, SPELLVALUE_BASE_POINT0, each, e, TRIG);
        }
        // else CANCEL / ENEMY_SPELL (dispel) / DEFAULT (evade): purged already, no detonation.
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_demonology_doombrand_debuff::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_demonology_doombrand()
{
    RegisterSpellScript(spell_demonology_doombrand);
    RegisterSpellScript(spell_demonology_doombrand_debuff);
}
