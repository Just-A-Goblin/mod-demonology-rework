/*
 * mod-demonology-rework — Demonic Empowerment + the Phase 4 spine.
 *
 * DBC spell 290000 is a dummy shell; this SpellScript applies the internal buff
 * (290500) to the caster's demons. The buff's amounts + duration are set dynamically
 * per the spine talents: Cruelty of the Pit (damage), Shadowflame Legion (absorb),
 * Unholy Vigor + Supreme Empowerment (duration). Supreme Empowerment also gates the
 * temp demons (Wild Imps + greater demon). Ruinous Empowerment adds a refresh-on-expiry
 * chance here (its leech lives in the demon-damage hook).
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellScript.h"
#include "Util.h"

#include <list>

using namespace Demonology;

namespace
{
    // Apply the empowerment buff to one demon with spine-adjusted amounts + duration. Shared
    // by the cast handler and the Ruinous Empowerment refresh. AddAura lands the buff DIRECTLY
    // on the demon (its effects target unit_caster, so CastSpell would hit the player instead).
    void EmpowerDemon(Player* owner, Unit* demon)
    {
        if (!owner || !demon || !demon->IsAlive())
            return;

        Aura* aura = owner->AddAura(SPELL_DEMONIC_EMPOWERMENT_BUFF, demon);
        if (!aura)
            return;

        // Effect 0 = +damage% (base + Cruelty of the Pit); effect 1 = +haste%; effect 2 =
        // absorb shield (Shadowflame Legion, % of the demon's max HP — 0/absent without sl).
        if (AuraEffect* e = aura->GetEffect(0))
            e->ChangeAmount(int32((gConfig.DemonicEmpowermentDamage + CrueltyOfThePitDamage(owner)) * 100.0f));
        if (AuraEffect* e = aura->GetEffect(1))
            e->ChangeAmount(int32(gConfig.DemonicEmpowermentHaste * 100.0f));
        if (AuraEffect* e = aura->GetEffect(2))
            e->SetAmount(int32(ShadowflameLegionAbsorb(owner) * float(demon->GetMaxHealth())));

        int32 const dur = int32(gConfig.DemonicEmpowermentDurationMs)
            + int32(UnholyVigorDurationMs(owner)) + int32(SupremeEmpowermentDurationMs(owner));
        aura->SetMaxDuration(dur);
        aura->SetDuration(dur);
    }
}

class spell_demonology_demonic_empowerment : public SpellScript
{
    PrepareSpellScript(spell_demonology_demonic_empowerment);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;

        // Permanent commanded demons: the anchor pet + every legionnaire — always empowered.
        EmpowerDemon(caster, caster->GetPet());
        Demonology::CommandPool* pool = sCommandPoolMgr->Find(caster->GetGUID());
        if (pool)
            for (ObjectGuid guid : pool->Legionnaires())
                EmpowerDemon(caster, ObjectAccessor::GetCreature(*caster, guid));

        // Temp demons (Wild Imps + the greater demon) are gated behind Supreme Empowerment.
        if (Demonology::SupremeEmpowermentTrained(caster))
        {
            if (pool)
                EmpowerDemon(caster, ObjectAccessor::GetCreature(*caster, pool->GreaterDemonGuid()));

            std::list<Creature*> temps;
            caster->GetCreatureListWithEntryInGrid(temps, NPC_WILD_IMP, 100.0f);
            for (Creature* imp : temps)
                if (imp->GetOwnerGUID() == caster->GetGUID())
                    EmpowerDemon(caster, imp);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_demonic_empowerment::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Ruinous Empowerment (re): when the empowerment buff EXPIRES, a chance to refresh it on that
// demon instead (the "no-expire" half; the leech half is in the demon-damage hook).
class spell_demonology_empowerment_buff : public AuraScript
{
    PrepareAuraScript(spell_demonology_empowerment_buff);

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;
        Player* owner = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* demon = GetUnitOwner();
        if (!owner || !demon)
            return;
        if (float const chance = Demonology::RuinousEmpowermentNoExpire(owner))
            if (roll_chance_f(chance * 100.0f))
                EmpowerDemon(owner, demon);         // refresh — its own expiry rolls again
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_demonology_empowerment_buff::AfterRemove, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL);
    }
};

void AddSC_demonology_empowerment_spells()
{
    RegisterSpellScript(spell_demonology_demonic_empowerment);
    RegisterSpellScript(spell_demonology_empowerment_buff);
}
