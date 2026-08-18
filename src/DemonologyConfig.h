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

        // Cruel Master (T2)
        float CruelMasterCritMultiplier  = 2.0f;

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

        bool  DebugLogShardIncome        = false;
    };

    // Global, refreshed on OnAfterConfigLoad.
    extern ModuleConfig gConfig;
}

#endif // MOD_DEMONOLOGY_REWORK_CONFIG_H
