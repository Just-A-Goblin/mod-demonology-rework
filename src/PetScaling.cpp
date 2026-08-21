/*
 * mod-demonology-rework — PetScaling implementation (Phase 6).
 */
#include "PetScaling.h"
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <unordered_map>
#include <unordered_set>

namespace Demonology::PetScaling
{
    namespace
    {
        // Demon base-attack interval from the template, hasted by Savage Instincts plus any
        // transient Bound by Blood window on the owner's pool. Re-derived each call so it
        // never compounds and reverts cleanly when the sources drop.
        uint32 DerivedAttackTime(Player* owner, Creature* demon)
        {
            uint32 baseTime = demon->GetCreatureTemplate() ? demon->GetCreatureTemplate()->BaseAttackTime : uint32(BASE_ATTACK_TIME);
            if (!baseTime)
                baseTime = uint32(BASE_ATTACK_TIME);
            float haste = DemonHastePct(owner) / 100.0f;
            if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
                haste += pool->LegionBuffHaste();
            return uint32(float(baseTime) / (1.0f + haste));
        }

        // Warded Legion CC-immunity bookkeeping: demons we've applied Fear/Charm/Poly immunity
        // to (ApplySpellImmune stacks entries on repeat, so track and only toggle on change).
        std::unordered_set<ObjectGuid> g_wlCcImmune;

        // Warded Legion (wl): apply the demon spell-resist ward (amount live per rank) and, at
        // rank 2, Fear/Charm/Polymorph immunity. Called every mirror tick for the anchor pet, so
        // it must be a NO-OP when nothing changed — re-casting the ward each tick replays the
        // clone's cast visual (the "continuous casting / purple skull" bug). Only (re)cast when
        // the ward is missing or its amount changed (rank change).
        void ApplyWardedLegion(Player* owner, Creature* demon)
        {
            float const resist = WardedLegionResist(owner);
            if (resist > 0.0f)
            {
                int32 const want = -int32(resist * 100.0f);
                Aura* a = demon->GetAura(SPELL_WARDED_LEGION_WARD);
                AuraEffect* eff = a ? a->GetEffect(EFFECT_0) : nullptr;
                if (!eff || eff->GetAmount() != want)
                    demon->CastCustomSpell(SPELL_WARDED_LEGION_WARD, SPELLVALUE_BASE_POINT0, want, demon, true);
            }
            else if (demon->HasAura(SPELL_WARDED_LEGION_WARD))
                demon->RemoveAurasDueToSpell(SPELL_WARDED_LEGION_WARD);

            bool const wantCc = WardedLegionCcImmune(owner);
            bool const hasCc  = g_wlCcImmune.count(demon->GetGUID()) > 0;
            if (wantCc && !hasCc)
            {
                for (uint32 m : { uint32(MECHANIC_FEAR), uint32(MECHANIC_CHARM), uint32(MECHANIC_POLYMORPH) })
                    demon->ApplySpellImmune(SPELL_WARDED_LEGION_WARD, IMMUNITY_MECHANIC, m, true);
                g_wlCcImmune.insert(demon->GetGUID());
            }
            else if (!wantCc && hasCc)
            {
                for (uint32 m : { uint32(MECHANIC_FEAR), uint32(MECHANIC_CHARM), uint32(MECHANIC_POLYMORPH) })
                    demon->ApplySpellImmune(SPELL_WARDED_LEGION_WARD, IMMUNITY_MECHANIC, m, false);
                g_wlCcImmune.erase(demon->GetGUID());
            }
        }
    }

    void ReapplyAttackSpeed(Player* owner, Creature* demon)
    {
        if (!owner || !demon || !demon->IsInWorld())
            return;
        demon->SetAttackTime(BASE_ATTACK, DerivedAttackTime(owner, demon));
    }

    int32 OwnerSpellPower(Player* owner)
    {
        if (!owner)
            return 0;
        // Base spell damage bonus for the shadow school captures universal +SP
        // (the IsPlayer() branch folds in gear, stat-based SP and AP conversions).
        return std::max<int32>(0, owner->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_SHADOW));
    }

    void ApplyInheritance(Player* owner, Creature* demon)
    {
        if (!owner || !demon || !demon->IsInWorld())
            return;

        int32 const sp = OwnerSpellPower(owner);

        // --- Health: a fraction of the owner's, so it scales with stamina/gear. ---
        // Preserve the current health% so re-applying mid-fight doesn't heal/hurt.
        // Fel Conditioning / Cursed Vitality add demon health via DemonHealthMult.
        if (gConfig.InheritHealthPctOfOwner > 0.0f)
        {
            uint32 const newMax = std::max<uint32>(1, uint32(float(owner->GetMaxHealth()) * gConfig.InheritHealthPctOfOwner * DemonHealthMult(owner)));
            float const curPct = demon->GetMaxHealth() ? float(demon->GetHealth()) / float(demon->GetMaxHealth()) : 1.0f;
            demon->SetCreateHealth(newMax);
            demon->SetMaxHealth(newMax);
            demon->SetHealth(std::max<uint32>(1, uint32(float(newMax) * curPct)));
        }

        // --- Melee: per-swing weapon damage derived from owner SP (SP -> melee). ---
        // AP on these creatures is ~0, so CalculateMinMaxDamage returns ~= the weapon
        // range we set here. Vicious Pact / Fel Armory scale the DAMAGE DEALT in the
        // demonology_demon_damage UnitScript (uniform with the pet), not here.
        float const perSwing = float(sp) * gConfig.InheritMeleeDamagePerSP;
        if (perSwing > 0.0f)
        {
            demon->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, perSwing * 0.85f);
            demon->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, perSwing * 1.15f);
            demon->UpdateDamagePhysical(BASE_ATTACK);
        }

        // --- Attack speed: Savage Instincts (si) + transient Bound by Blood haste,
        // re-derived from the template base so it never compounds and resets when dropped. ---
        demon->SetAttackTime(BASE_ATTACK, DerivedAttackTime(owner, demon));

        // --- Spell crit: the core creature base (5%, set in the Unit ctor) plus Pactbound
        // Fury, so the demon's Firebolt / Doom Bolt land REAL, visible spell crits — the
        // spell-side match to the melee crit hook. Absolute set (base + pf) so it never
        // compounds; re-applied on summon / SP change / respec. (Needs core patch 01.) ---
        demon->SetBaseSpellCritChance(5 + int32(PactboundFuryCritChance(owner) * 100.0f));

        // Warded Legion (wl): spell-resist ward + rank-2 CC immunity.
        ApplyWardedLegion(owner, demon);
    }

    // ---- Anchor pet (core Pet) talent buffs ----
    // Legionnaires are plain creatures we fully own, so we SET their stats above. The
    // anchor pet is a core-managed Pet whose stats the core recomputes, so instead we
    // apply health / attack-speed as PERCENT modifiers that survive those recalcs, with
    // per-owner bookkeeping so re-applying only moves the delta (no drift, preserves
    // other buffs). Its DAMAGE is handled by the demon_damage UnitScript like the rest.
    namespace
    {
        struct AppliedPetMods { ObjectGuid petGuid; float healthMult = 1.0f; float meleeDmgMult = 1.0f; float castHastePct = 0.0f; };
        std::unordered_map<ObjectGuid, AppliedPetMods> g_petMods;   // keyed by OWNER guid
    }

    void ApplyPetMods(Player* owner)
    {
        if (!owner)
            return;
        Pet* pet = owner->GetPet();
        if (!pet)
            return;

        // Health (Fel Conditioning / Cursed Vitality): a percent modifier that the core
        // includes in every UpdateMaxHealth. Delta-ratio bookkeeping so it never drifts
        // and preserves other buffs. (Damage is handled by the demon_damage UnitScript.)
        AppliedPetMods& cur = g_petMods[owner->GetGUID()];
        if (cur.petGuid != pet->GetGUID())      // new pet — its modifier groups start clean
            cur = { pet->GetGUID(), 1.0f };

        float const wantHealth = DemonHealthMult(owner);
        if (std::fabs(wantHealth - cur.healthMult) > 0.001f)
        {
            pet->ApplyStatPctModifier(UNIT_MOD_HEALTH, TOTAL_PCT, (wantHealth / cur.healthMult - 1.0f) * 100.0f);
            cur.healthMult = wantHealth;
        }

        // Vicious Pact + Fel Armory (melee halves): a real PERCENT damage modifier on the pet's
        // mainhand, so the FULL % shows in the pet tab's DPS and matches the tooltip ("X% increased
        // damage") — NOT the old SP->AP flat value, which was tiny at low spell power. Delta-ratio
        // bookkeeping (like health) so it never compounds/drifts and preserves the pet's other damage
        // mods; tracks rank/respec/armor-up changes each Mirror tick. EXCLUDED from the swing-time
        // multiplier (DemonDamageMult anchorPet=true) so it isn't counted twice; spell halves stay
        // swing-time (this modifier is melee-only).
        float const wantMeleeMult = 1.0f + Demonology::AnchorPetMeleeDamagePct(owner);
        if (std::fabs(wantMeleeMult - cur.meleeDmgMult) > 0.001f)
        {
            pet->ApplyStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, (wantMeleeMult / cur.meleeDmgMult - 1.0f) * 100.0f);
            cur.meleeDmgMult = wantMeleeMult;
        }

        // Attack speed (Savage Instincts + transient Bound by Blood): set the base attack
        // time from the template, re-derived each call so it never compounds and shows in
        // GetAttackTime (same as legionnaires). Haste auras still multiply on top.
        pet->SetAttackTime(BASE_ATTACK, DerivedAttackTime(owner, pet));

        // Savage Instincts CASTER half for the ANCHOR pet: the guardian imps/Doomguard get their
        // recast timers hastened in GuardianAttackerAI, but the anchor pet is core-managed (PetAI), so
        // instead speed its SPELL CASTS via cast-speed haste — same si + Bound-by-Blood value as melee.
        // Keeps an anchor Imp's Firebolt consistent with Wild Imp / Imp legionnaire Firebolts. Delta-
        // applied (remove old %, apply new %) so it never compounds and preserves other haste sources.
        float wantCastHastePct = DemonHastePct(owner);                         // si, as a percent
        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
            wantCastHastePct += pool->LegionBuffHaste() * 100.0f;             // bbb, fraction -> percent
        if (std::fabs(wantCastHastePct - cur.castHastePct) > 0.01f)
        {
            if (cur.castHastePct > 0.0f)
                pet->ApplyCastTimePercentMod(cur.castHastePct, false);        // remove previous contribution
            if (wantCastHastePct > 0.0f)
                pet->ApplyCastTimePercentMod(wantCastHastePct, true);         // apply current (faster casts)
            cur.castHastePct = wantCastHastePct;
        }

        // Warded Legion (wl): spell-resist ward + rank-2 CC immunity (same as legionnaires).
        ApplyWardedLegion(owner, pet);
    }

    void ForgetPet(ObjectGuid owner)
    {
        g_petMods.erase(owner);
    }

    void ReapplyAll(Player* owner)
    {
        if (!owner)
            return;

        ApplyPetMods(owner);            // anchor pet health/attack-speed talent buffs

        // Pool legionnaires (persistent).
        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
            for (ObjectGuid guid : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
                    ApplyInheritance(owner, c);

        // Temporary Wild Imps in range.
        std::list<Creature*> imps;
        owner->GetCreatureListWithEntryInGrid(imps, NPC_WILD_IMP, 100.0f);
        for (Creature* imp : imps)
            if (imp->GetOwnerGUID() == owner->GetGUID())
                ApplyInheritance(owner, imp);
    }
}
