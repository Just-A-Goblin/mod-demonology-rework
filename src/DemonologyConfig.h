/*
 * mod-demonology-rework — config accessor.
 *
 * Every design number lives here so it stays tunable without a recompile
 * (see PLAN §10.2). Populated from mod_demonology_rework.conf on config load.
 */
#ifndef MOD_DEMONOLOGY_REWORK_CONFIG_H
#define MOD_DEMONOLOGY_REWORK_CONFIG_H

#include "Define.h"   // AzerothCore's uint8/uint32 typedefs

namespace Demonology
{
    struct ModuleConfig
    {
        bool  Enable = true;

        // Soul Harvest (T1) — shard economy throttle
        float SoulHarvestChancePerRank   = 0.04f;  // 4/8/12% at ranks 1/2/3
        uint32 SoulHarvestInternalCdMs   = 1000;

        // Cruel Master (T2) — demon crits accelerate Soul Harvest. The crit is SIMULATED in
        // the Soul Harvest proc (no server hook reports a crit result): roll the demon's crit
        // chance (Pactbound Fury + a base), and on a "crit" rank 1 multiplies the proc chance,
        // rank 2 also multiplies down the ICD.
        float CruelMasterProcChanceMult  = 2.0f;   // rank>=1: x proc chance on a sim-crit
        float CruelMasterIcdMultOnCrit   = 0.5f;   // rank>=2: x ICD on a sim-crit (<1 = shorter)
        float CruelMasterBaseCritChance  = 0.05f;  // demon base crit added to Pactbound Fury for the sim

        // Per-rank talent multipliers (source of truth = conf; comma lists in the .dist).
        // Demon DAMAGE, additive on 1.0:
        float VpMeleePct[3]       = { 0.08f, 0.16f, 0.24f };  // Vicious Pact — SP-as-AP on melee
        float VpSpellPct[3]       = { 0.05f, 0.10f, 0.15f };  // Vicious Pact — SP-as-SP on spells
        float FaDamagePct[3]      = { 0.05f, 0.10f, 0.15f };  // Fel Armory — while owner Fel Armor up
        // Demon HEALTH, additive on 1.0:
        float FcHealthPct[3]      = { 0.05f, 0.10f, 0.15f };  // Fel Conditioning
        float CvDemonHealthPct[2] = { 0.06f, 0.12f };         // Cursed Vitality — demon-stamina part
        // Demon melee HASTE, percent (12 = +12% attack speed):
        float SiHastePct[3]       = { 4.0f, 8.0f, 12.0f };    // Savage Instincts

        // Wild Imps
        uint32 WildImpCount              = 3;
        uint32 WildImpDurationMs         = 20000;
        float  WildImpSPCoefficient      = 0.28f;
        uint32 WildImpShardCost          = 2;   // Path B: Summon Wild Imps costs 2 Soul Shards
        uint32 ImprovedLegionWildImpShardReduction = 1;  // il trims that cost (2 -> 1)

        // Greater demons (Infernal / Doomguard) — temporary cooldown guardians
        uint32 InfernalDurationMs        = 60000;
        uint32 DoomguardDurationMs       = 60000;
        float  DoomBoltSPCoefficient     = 0.64f;  // Doom Bolt SP scaling per bolt (deployed value)
        uint32 DoomBoltBaseDamage        = 850;    // SINGLE-TARGET nuke — the bigger hit (overrides the cloned 40876 base ~2249)
        // Doom Blast (40878) is the AOE second hit Doom Bolt triggers; kept BELOW the
        // single-target bolt so the cleave isn't the stronger effect.
        uint32 DoomBlastBaseDamage       = 300;    // AOE — the smaller hit
        float  DoomBlastSPCoefficient    = 0.50f;
        float  InfernalScale             = 0.70f;  // shrink the oversized Infernal model (1.0 = model default)
        // Command-slot cost while a greater demon is active (they crowd out legionnaires,
        // so they can't just stack on a full legion). Summoning one needs this many total
        // command slots and evicts the oldest legionnaires to fit.
        uint8  InfernalCommandSlots      = 2;
        uint8  DoomguardCommandSlots     = 3;

        // Demonic Empowerment
        float DemonicEmpowermentHaste    = 0.30f;
        float DemonicEmpowermentDamage    = 0.20f;
        uint32 DemonicEmpowermentDurationMs = 12000;

        // Command Pool (Phase 1 + Phase 5 Command talents)
        uint8 PoolBaseLegionnaires = 0;  // slots before talents (design: base 0 — Command talents grant every slot)
        uint8 PoolMaxLegionnaires = 4;   // hard cap ceiling = Expanded Command/II + Legion Commander (+headroom)
        uint32 SummonLegionnaireShardCost = 1;   // Soul Shards per per-type legion summon

        // Stat inheritance (Phase 6) — demons scale off the owner instead of a flat
        // clamp. Warlocks are casters, so demons derive their power from owner SP.
        float InheritHealthPctOfOwner = 0.60f;   // demon max health = this * owner max health
        float InheritMeleeDamagePerSP = 0.15f;   // per-swing melee weapon damage = this * owner SP

        float LegionStandardHealthPctOfOwner = 0.40f;
        float ThreatNonAnchorMultiplier      = 0.50f;
        float LegionAuraDamagePct            = 0.05f;
        float LegionAuraHastePct             = 0.05f;
        float BeaconDemonDamagePct           = 0.50f;

        // --- Phase 1 talents ---
        // Pactbound Fury (pf, T3) — demons roll a crit at hit time in the damage hook.
        float PactboundFuryCritChancePct[3] = { 0.02f, 0.04f, 0.06f };  // per rank
        float PactboundFuryCritMultiplier   = 2.0f;                     // crit damage x

        // Demonic Rebirth (dr, T6) — chance to instantly resummon a dying legionnaire.
        float  DemonicRebirthChancePct[2]   = { 0.50f, 1.00f };         // per rank
        uint32 DemonicRebirthIcdMs          = 60000;

        // Bound by Blood (bbb, T9) — on demon death, survivors gain a transient buff and
        // the owner refunds a Soul Shard (Path B: "demon deaths fund your actives").
        float  BoundByBloodDamagePct[2]     = { 0.15f, 0.30f };         // +demon damage (fraction)
        float  BoundByBloodHastePct[2]      = { 0.25f, 0.45f };         // +demon attack speed (fraction)
        uint32 BoundByBloodDurationMs       = 10000;
        uint32 BoundByBloodIcdMs            = 180000;                   // buff cooldown (it's a strong bloodlust); shard refund is NOT gated
        bool   BoundByBloodRefundShard      = true;

        // Overlord's Presence (op, T8) — per commanded demon, buff the OWNER.
        float OverlordsPresenceHealthPct[3] = { 0.02f, 0.04f, 0.06f };  // per rank, per demon
        float OverlordsPresenceHastePct[3]  = { 0.015f, 0.03f, 0.045f };// per rank, per demon

        // Cursed Vitality (cv, T1) — owner-stamina half (demon half is in DemonHealthMult).
        float CursedVitalityOwnerStaminaPct[2] = { 0.03f, 0.06f };      // per rank

        // Legion Aura (gwd groundwork) — party aura toggles on at this pool size
        // (DESIGN_V2 §8.1: >=1 commanded demon). Amounts reuse LegionAura.DamagePct/HastePct.
        uint32 LegionAuraMinDemons          = 1;

        bool  DebugLogShardIncome        = false;
    };

    // Global, refreshed on OnAfterConfigLoad.
    extern ModuleConfig gConfig;
}

#endif // MOD_DEMONOLOGY_REWORK_CONFIG_H
