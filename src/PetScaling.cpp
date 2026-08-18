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
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <unordered_map>

namespace Demonology::PetScaling
{
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

        // --- Attack speed: Savage Instincts (si) hastes demon swings. Re-derived from
        // the template base each apply, so it never compounds and resets when dropped. ---
        {
            uint32 baseTime = demon->GetCreatureTemplate() ? demon->GetCreatureTemplate()->BaseAttackTime : uint32(BASE_ATTACK_TIME);
            if (!baseTime)
                baseTime = uint32(BASE_ATTACK_TIME);
            float const haste = DemonHastePct(owner) / 100.0f;
            demon->SetAttackTime(BASE_ATTACK, uint32(float(baseTime) / (1.0f + haste)));
        }
    }

    // ---- Anchor pet (core Pet) talent buffs ----
    // Legionnaires are plain creatures we fully own, so we SET their stats above. The
    // anchor pet is a core-managed Pet whose stats the core recomputes, so instead we
    // apply health / attack-speed as PERCENT modifiers that survive those recalcs, with
    // per-owner bookkeeping so re-applying only moves the delta (no drift, preserves
    // other buffs). Its DAMAGE is handled by the demon_damage UnitScript like the rest.
    namespace
    {
        struct AppliedPetMods { ObjectGuid petGuid; float healthMult = 1.0f; };
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

        // Attack speed (Savage Instincts): set the base attack time from the template,
        // re-derived each call so it never compounds and shows in GetAttackTime (same as
        // legionnaires). Haste auras still multiply on top via m_modAttackSpeedPct.
        uint32 baseTime = pet->GetCreatureTemplate() ? pet->GetCreatureTemplate()->BaseAttackTime : uint32(BASE_ATTACK_TIME);
        if (!baseTime)
            baseTime = uint32(BASE_ATTACK_TIME);
        pet->SetAttackTime(BASE_ATTACK, uint32(float(baseTime) / (1.0f + DemonHastePct(owner) / 100.0f)));
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
