/*
 * mod-demonology-rework — talent helpers.
 *
 * Small inline readers used to wire self-contained talent effects into the demon
 * systems. Talent RANKS live in the talent map (not the spell book — our markers
 * have addToSpellBook=0), so they must be read via Player::HasTalent, and a node's
 * ranks are allocated as CONSECUTIVE spell ids from its rank-1 marker.
 */
#ifndef MOD_DEMONOLOGY_REWORK_TALENTS_H
#define MOD_DEMONOLOGY_REWORK_TALENTS_H

#include "Define.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "Creature.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellMgr.h"

namespace Demonology
{
    // The warlock owner of an owned demon (anchor pet, legionnaire, or Wild Imp), or
    // nullptr if the unit isn't a creature owned by a warlock. Used to scale demon
    // damage from any of them uniformly.
    inline Player* WarlockOwnerOfDemon(Unit* demon)
    {
        if (!demon || demon->GetTypeId() != TYPEID_UNIT)
            return nullptr;
        Unit* owner = demon->GetOwner();
        Player* p = owner ? owner->ToPlayer() : nullptr;
        return (p && p->getClass() == CLASS_WARLOCK) ? p : nullptr;
    }

    // Trained rank of a talent: 0 = untrained, else 1..maxRank. Ranks are consecutive
    // marker spells starting at rank1Spell (see the header note).
    inline uint8 TalentRank(Player* owner, uint32 rank1Spell, uint8 maxRank)
    {
        if (!owner)
            return 0;
        uint8 const spec = owner->GetActiveSpec();
        for (uint8 r = maxRank; r >= 1; --r)
            if (owner->HasTalent(rank1Spell + (r - 1), spec))
                return r;
        return 0;
    }

    // Is any warlock armor buff active on the owner? Fel Armory keys off it — Demon Skin and Demon
    // Armor count too, not just Fel Armor. Checked by FIRST-IN-CHAIN so every rank matches without
    // enumerating them (Demon Armor alone has 8 ranks; hardcoding missed rank 7 = 47793).
    inline bool OwnerHasFelArmor(Player* owner)
    {
        for (auto const& pair : owner->GetAppliedAuras())
        {
            uint32 const first = sSpellMgr->GetFirstSpellInChain(pair.first);
            if (first == 687        // Demon Skin  (all ranks)
             || first == 706        // Demon Armor (all ranks)
             || first == 28176)     // Fel Armor   (all ranks)
                return true;
        }
        return owner->HasAura(44520) || owner->HasAura(44977);  // stray Fel Armor variants not in the rank chain
    }

    // Combined demon DAMAGE multiplier from the scaling talents, additive on top of 1.0:
    //   Vicious Pact  — SP-as-attack-power on melee ([8,16,24]%) or SP-as-spell-power on
    //                   spells ([5,10,15]%); pass meleeSide to pick which.
    //   Fel Armory    — [5,10,15]% while the owner's Demon Skin / Demon Armor / Fel Armor is up.
    inline float DemonDamageMult(Player* owner, bool meleeSide, bool anchorPet = false)
    {
        if (!owner)
            return 1.0f;
        float mult = 1.0f;
        if (uint8 vp = TalentRank(owner, SPELL_TALENT_VICIOUS_PACT, 3))
        {
            // Vicious Pact's MELEE half is real Attack Power on the anchor pet (SP->AP, applied in
            // PetScaling so it shows in the pet tab and drives damage) — exclude it from the anchor's
            // swing-time multiplier to avoid double-counting. Guardians (ToPet()==null) still get it
            // here, and the SPELL half stays swing-time for everyone (pets have no settable SP field).
            if (meleeSide)
            {
                if (!anchorPet)
                    mult += gConfig.VpMeleePct[vp - 1];
            }
            else
                mult += gConfig.VpSpellPct[vp - 1];
        }
        // Fel Armory (while an armor buff is up): its MELEE half is also real Attack Power on the
        // anchor pet (like Vicious Pact) — skip it here for the anchor's melee. Spell half + guardians
        // stay swing-time.
        if (uint8 fa = TalentRank(owner, SPELL_TALENT_FEL_ARMORY, 3))
            if (OwnerHasFelArmor(owner))
            {
                if (meleeSide)
                {
                    if (!anchorPet)
                        mult += gConfig.FaDamagePct[fa - 1];
                }
                else
                    mult += gConfig.FaDamagePct[fa - 1];
            }
        return mult;
    }

    // Anchor pet's talent MELEE damage bonus as a FRACTION (e.g. 0.24): Vicious Pact's melee % always
    // + Fel Armory's melee % while an armor buff is up. Applied as a real UNIT_MOD_DAMAGE_MAINHAND
    // TOTAL_PCT in PetScaling (shows the FULL % in the pet tab's DPS, matches the tooltip), excluded
    // from the swing-time multiplier above (anchorPet=true). Spell halves stay swing-time.
    inline float AnchorPetMeleeDamagePct(Player* owner)
    {
        float pct = 0.0f;
        if (uint8 vp = TalentRank(owner, SPELL_TALENT_VICIOUS_PACT, 3))
            pct += gConfig.VpMeleePct[vp - 1];
        if (uint8 fa = TalentRank(owner, SPELL_TALENT_FEL_ARMORY, 3))
            if (OwnerHasFelArmor(owner))
                pct += gConfig.FaDamagePct[fa - 1];
        return pct;
    }

    // Demon HEALTH multiplier (>=1.0): Fel Conditioning +[5,10,15]% and Cursed Vitality's
    // demon-stamina part +[6,12]% (the owner-stamina part is a separate player buff in OwnerMods.cpp).
    inline float DemonHealthMult(Player* owner)
    {
        if (!owner)
            return 1.0f;
        float mult = 1.0f;
        if (uint8 fc = TalentRank(owner, SPELL_TALENT_FEL_CONDITIONING, 3))
            mult += gConfig.FcHealthPct[fc - 1];
        if (uint8 cv = TalentRank(owner, SPELL_TALENT_CURSED_VITALITY, 2))
            mult += gConfig.CvDemonHealthPct[cv - 1];
        return mult;
    }

    // Demon melee-haste PERCENT (e.g. 12.0 for +12% attack speed) from Savage Instincts.
    inline float DemonHastePct(Player* owner)
    {
        if (uint8 si = TalentRank(owner, SPELL_TALENT_SAVAGE_INSTINCTS, 3))
            return gConfig.SiHastePct[si - 1];
        return 0.0f;
    }

    // --- Phase 1 talent readers ---

    // Pactbound Fury (pf): demon crit chance as a fraction (0 = untrained).
    inline float PactboundFuryCritChance(Player* owner)
    {
        if (uint8 pf = TalentRank(owner, SPELL_TALENT_PACTBOUND_FURY, 3))
            return gConfig.PactboundFuryCritChancePct[pf - 1];
        return 0.0f;
    }

    // Demonic Rebirth (dr): instant-resummon chance as a fraction (0 = untrained).
    inline float DemonicRebirthChance(Player* owner)
    {
        if (uint8 dr = TalentRank(owner, SPELL_TALENT_DEMONIC_REBIRTH, 2))
            return gConfig.DemonicRebirthChancePct[dr - 1];
        return 0.0f;
    }

    // Overlord's Presence (op): owner buff PER commanded demon (fractions; 0 = untrained).
    inline float OverlordsPresenceHealthPerDemon(Player* owner)
    {
        if (uint8 op = TalentRank(owner, SPELL_TALENT_OVERLORDS_PRESENCE, 3))
            return gConfig.OverlordsPresenceHealthPct[op - 1];
        return 0.0f;
    }
    inline float OverlordsPresenceHastePerDemon(Player* owner)
    {
        if (uint8 op = TalentRank(owner, SPELL_TALENT_OVERLORDS_PRESENCE, 3))
            return gConfig.OverlordsPresenceHastePct[op - 1];
        return 0.0f;
    }

    // Cursed Vitality (cv): owner-stamina half as a fraction (0 = untrained).
    inline float CursedVitalityOwnerStamina(Player* owner)
    {
        if (uint8 cv = TalentRank(owner, SPELL_TALENT_CURSED_VITALITY, 2))
            return gConfig.CursedVitalityOwnerStaminaPct[cv - 1];
        return 0.0f;
    }

    // Bound by Blood (bbb): survivor buff amounts as fractions (0 = untrained).
    inline float BoundByBloodDamage(Player* owner)
    {
        if (uint8 bbb = TalentRank(owner, SPELL_TALENT_BOUND_BY_BLOOD, 2))
            return gConfig.BoundByBloodDamagePct[bbb - 1];
        return 0.0f;
    }
    inline float BoundByBloodHaste(Player* owner)
    {
        if (uint8 bbb = TalentRank(owner, SPELL_TALENT_BOUND_BY_BLOOD, 2))
            return gConfig.BoundByBloodHastePct[bbb - 1];
        return 0.0f;
    }

    // --- Phase 4: Demonic Empowerment spine ---
    // Unholy Vigor (uv): extra empowerment duration in ms.
    inline uint32 UnholyVigorDurationMs(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_UNHOLY_VIGOR, 3) * gConfig.UnholyVigorDurationMsPerRank;
    }
    // Cruelty of the Pit (cotp): extra empowered-demon damage (fraction).
    inline float CrueltyOfThePitDamage(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_CRUELTY_OF_THE_PIT, 3))
            return gConfig.CrueltyOfThePitDamagePct[r - 1];
        return 0.0f;
    }
    // Ruinous Empowerment (re): owner leech fraction + buff refresh (no-expire) chance.
    inline float RuinousEmpowermentLeech(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_RUINOUS_EMPOWERMENT, 3))
            return gConfig.RuinousEmpowermentLeechPct[r - 1];
        return 0.0f;
    }
    inline float RuinousEmpowermentNoExpire(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_RUINOUS_EMPOWERMENT, 3))
            return gConfig.RuinousEmpowermentNoExpirePct[r - 1];
        return 0.0f;
    }
    // Supreme Empowerment (se): extra duration in ms; and whether temp demons are empowered.
    inline uint32 SupremeEmpowermentDurationMs(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_SUPREME_EMPOWERMENT, 2) * gConfig.SupremeEmpowermentDurationMsPerRank;
    }
    inline bool SupremeEmpowermentTrained(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_SUPREME_EMPOWERMENT, 2) > 0;
    }
    // Shadowflame Legion (sl): empowerment shield as a fraction of the demon's max HP.
    inline float ShadowflameLegionAbsorb(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_SHADOWFLAME_LEGION, 2))
            return gConfig.ShadowflameLegionAbsorbPct[r - 1];
        return 0.0f;
    }

    // Cruel Master (cm): trained rank (0/1/2).
    inline uint8 CruelMasterRank(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_CRUEL_MASTER, 2);
    }

    // The demon crit chance Cruel Master simulates in the Soul Harvest proc: the
    // Pactbound Fury talent crit plus a base (fraction). Used only for cm's roll.
    inline float DemonSimCritChance(Player* owner)
    {
        return PactboundFuryCritChance(owner) + gConfig.CruelMasterBaseCritChance;
    }

    // --- Phase 6 talent readers ---

    // Is a creature one of our greater demons (Infernal / Doomguard)? Beacon of Ruin keys off it.
    inline bool IsGreaterDemon(Unit const* demon)
    {
        return demon && (demon->GetEntry() == NPC_BASE_INFERNAL || demon->GetEntry() == NPC_BASE_DOOMGUARD);
    }

    // Beacon of Ruin (bor): extra damage fraction a greater demon deals (0 if untrained/not one).
    inline float BeaconOfRuinDamage(Player* owner, Unit const* attacker)
    {
        if (!IsGreaterDemon(attacker))
            return 0.0f;
        if (uint8 r = TalentRank(owner, SPELL_TALENT_BEACON_OF_RUIN, 2))
            return gConfig.BeaconOfRuinDamagePct[r - 1];
        return 0.0f;
    }
    // Beacon of Ruin (bor): fraction the greater-demon summon cooldown is cut by (0 if untrained).
    inline float BeaconOfRuinCdReduction(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_BEACON_OF_RUIN, 2))
            return gConfig.BeaconOfRuinCdReductionPct[r - 1];
        return 0.0f;
    }

    // Blood Tithe (bt): fraction of a demon's damage that heals the owner. Doubled once the owner
    // commands BloodTitheDoubleAtDemons or more demons. commandedCount is passed in (pool-derived).
    inline float BloodTitheHeal(Player* owner, uint8 commandedCount)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_BLOOD_TITHE, 2))
        {
            float frac = gConfig.BloodTitheHealPct[r - 1];
            if (commandedCount >= gConfig.BloodTitheDoubleAtDemons)
                frac *= 2.0f;
            return frac;
        }
        return 0.0f;
    }

    // Warded Legion (wl): demon spell-resist fraction (0 = untrained) + CC-immunity gate (rank 2).
    inline float WardedLegionResist(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_WARDED_LEGION, 2))
            return gConfig.WardedLegionResistPct[r - 1];
        return 0.0f;
    }
    inline bool WardedLegionCcImmune(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_WARDED_LEGION, 2) >= 2;
    }

    // Grim Bargain (gb): damage-buff fraction + proc chance (0 = untrained).
    inline float GrimBargainDamage(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_GRIM_BARGAIN, 2))
            return gConfig.GrimBargainDamagePct[r - 1];
        return 0.0f;
    }
    inline float GrimBargainProcChance(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_GRIM_BARGAIN, 2) ? gConfig.GrimBargainProcPct : 0.0f;
    }

    // Fel Conduit (fcd): per-demon-attack chance to grant a Conduit charge (0 = untrained).
    inline float FelConduitProc(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_FEL_CONDUIT, 2))
            return gConfig.FelConduitProcPct[r - 1];
        return 0.0f;
    }

    // Fel Blood (fb): Lash of Pain damage bonus (fraction; 0 = untrained).
    inline float FelBloodLash(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_FEL_BLOOD, 2))
            return gConfig.FelBloodLashPct[r - 1];
        return 0.0f;
    }

    // Fel Corruption (rc, REDESIGNED): effectiveness with which Corruption ticks feed Soul Harvest
    // + Doombrand (0 = untrained). Reuses the rc marker (SPELL_TALENT_RAPID_CONJURATION, 3 ranks).
    inline float FelCorruptionEffectiveness(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_RAPID_CONJURATION, 3))
            return gConfig.FelCorruptionRankEffectiveness[r - 1];
        return 0.0f;
    }

    // Vital Conduit (vc): fraction of Life Tap's sacrificed health that heals the legion (0 = untrained).
    inline float VitalConduitHeal(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_VITAL_CONDUIT, 2))
            return gConfig.VitalConduitHealPct[r - 1];
        return 0.0f;
    }

    // Riftwalker (rw): whether the demon-warp-on-teleport is trained.
    inline bool RiftwalkerTrained(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_RIFTWALKER, 1) > 0;
    }

    // Fervent Standard (fs) — rank-scaled percentages (config-only; the radius test lives in
    // FerventStandard.h where the circle GameObject is available). Return 0 when untalented.
    inline float FerventStandardDamagePct(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_FERVENT_STANDARD, 2))
            return gConfig.FerventStandardDamagePct[r - 1];
        return 0.0f;
    }
    inline float FerventStandardMitigationPct(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_FERVENT_STANDARD, 2))
            return gConfig.FerventStandardMitigationPct[r - 1];
        return 0.0f;
    }

    // Improved Wild Imps (iwi): extra Wild Imp duration (ms) + Firebolt 2nd-target chance (fraction).
    inline uint32 ImprovedWildImpsDurationMs(Player* owner)
    {
        return TalentRank(owner, SPELL_TALENT_IMPROVED_WILD_IMPS, 2) * gConfig.ImprovedWildImpsDurationMsPerRank;
    }
    inline float ImprovedWildImpsSecondTargetChance(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_IMPROVED_WILD_IMPS, 2))
            return gConfig.ImprovedWildImpsSecondTargetPct[r - 1];
        return 0.0f;
    }
    // Wrath of the Legion (wotl): Firebolt chance to spawn an extra Wild Imp (fraction).
    inline float WrathOfTheLegionSpawnChance(Player* owner)
    {
        if (uint8 r = TalentRank(owner, SPELL_TALENT_WRATH_OF_THE_LEGION, 3))
            return gConfig.WrathOfTheLegionSpawnPct[r - 1];
        return 0.0f;
    }

    // Soul Shard cost of Summon Wild Imps (Path B) — fixed at 1. The mana cost (30% base) and
    // Improved Legion's reduction are DBC (spell cost + SPELLMOD_COST), not C++.
    inline uint32 WildImpShardCost(Player* /*owner*/)
    {
        return gConfig.WildImpShardCost;
    }
}

#endif // MOD_DEMONOLOGY_REWORK_TALENTS_H
