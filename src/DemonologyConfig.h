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
        float  WildImpSPCoefficient      = 0.11f;   // Firebolt SP coef for TEMPORARY Wild Imps (FLATTEN 2026-08-20: 0.16->0.11)
        float  ImpLegionnaireSPCoefficient = 0.20f; // Firebolt SP coef for PERMANENT Imp legionnaires (FLATTEN: 0.30->0.20)
        uint32 WildImpShardCost          = 1;   // Path B: Summon Wild Imps costs 1 Soul Shard (fixed)
        // Summon Wild Imps' 30% base-mana cost + Improved Legion's -15/-30% reduction are now DBC
        // (a real spell cost + SPELLMOD_COST), not C++/config — see summon_wild_imps.yaml + the il node.
        // Summon Wild Imps (@10) + Demonic Empowerment (@50) are TRAINER-taught (base SQL
        // 33_baseline_trainer_spells.sql), not config/hybrid — the ReqLevel lives in that SQL.

        // Greater demons — DIFFERENTIATED roles (docs/slot_comparison.py):
        //   Doomguard = SINGLE-TARGET nuke (strong Doom Bolt, token Doom Blast cleave),
        //   Infernal  = AoE/tank (periodic fire nova via the Command Pulse; melee off owner SP).
        // Bases are SP-WEIGHTED (low flat + high coef) so they scale smoothly instead of
        // dominating at low level.
        uint32 InfernalDurationMs        = 60000;
        uint32 DoomguardDurationMs       = 60000;
        float  DoomBoltSPCoefficient     = 0.55f;  // Doomguard ST nuke (FLATTEN 2026-08-20: 1.00->0.55, flatter scaling)
        uint32 DoomBoltBaseDamage        = 220;    // raised flat base (FLATTEN: 80->220) so it holds at low level w/o spiking at 80
        // Doom Blast (40878) is now just a TOKEN cleave — Doomguard is deliberately ST-focused.
        uint32 DoomBlastBaseDamage       = 0;
        float  DoomBlastSPCoefficient    = 0.10f;
        uint32 InfernalPulseCooldownMs   = 5000;   // Infernal casts its fire nova (290503) this often (AoE identity)
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

        // --- Phase 4: Demonic Empowerment spine (all layer onto the 290500 buff) ---
        uint32 UnholyVigorDurationMsPerRank      = 1000;                 // uv: +1s per rank (3 ranks)
        float  CrueltyOfThePitDamagePct[3]       = { 0.05f, 0.10f, 0.15f }; // cotp: +demon damage while empowered
        float  RuinousEmpowermentLeechPct[3]     = { 0.07f, 0.14f, 0.20f }; // re: heal owner for % of empowered demon damage
        float  RuinousEmpowermentNoExpirePct[3]  = { 0.07f, 0.14f, 0.20f }; // re: chance the buff refreshes on expiry
        uint32 SupremeEmpowermentDurationMsPerRank = 3000;               // se: +3s per rank (2 ranks); also gates temp demons
        float  ShadowflameLegionAbsorbPct[2]     = { 0.15f, 0.30f };     // sl: absorb shield = % of the demon's max HP

        // --- Phase 5: Doombrand (§6) ---
        float  DoombrandStorePct   = 0.15f;   // fraction of demon damage to the branded target that's stored
        float  DoombrandCapSPCoef  = 6.0f;    // stored damage cap = this * owner spell power
        uint32 DoombrandShardCost  = 1;
        float  DoombrandAoeRadius  = 8.0f;    // death-detonation splits the stored damage among enemies within this radius

        // --- Phase 6 talents ---
        // Blood Tithe (bt): demon damage heals the owner for this fraction, doubled at DoubleAtDemons+.
        float  BloodTitheHealPct[2]      = { 0.04f, 0.08f };
        uint8  BloodTitheDoubleAtDemons  = 3;
        // Beacon of Ruin (bor): greater demons (Infernal/Doomguard) deal +damage and their
        // summon cooldown is cut by this fraction (ship low — DESIGN_V2 §4).
        float  BeaconOfRuinDamagePct[2]      = { 0.15f, 0.30f };
        float  BeaconOfRuinCdReductionPct[2] = { 0.20f, 0.40f };
        // Fel Corruption (rc, REDESIGNED): your Corruption ticks count as demon damage for Soul
        // Harvest + Doombrand at this effectiveness per rank. DoombrandStoreMult = the half-weight
        // factor on the brand charge (a deliberate secondary source; DESIGN_V2 §6.4).
        float  FelCorruptionRankEffectiveness[3] = { 0.33f, 0.66f, 1.00f };
        float  FelCorruptionDoombrandStoreMult   = 0.50f;
        // Improved Legion (il) is a single-effect node: it only trims the Wild Imp shard cost
        // (ImprovedLegionWildImpShardReduction, above). The old cast-time-reduction half was inert
        // (never wired) and was removed with Addendum A.4.
        // Improved Wild Imps (iwi): Wild Imp duration bonus per rank (ms) + Firebolt 2nd-target chance per rank.
        uint32 ImprovedWildImpsDurationMsPerRank = 5000;
        float  ImprovedWildImpsSecondTargetPct[2] = { 0.10f, 0.20f };
        // Wrath of the Legion (wotl): Firebolt chance per rank to spawn an extra Wild Imp; cap per cast.
        float  WrathOfTheLegionSpawnPct[3] = { 0.10f, 0.20f, 0.30f };
        uint8  WrathOfTheLegionMaxChainsPerCast = 2;
        float  WrathOfTheLegionManaCostPct = 5.0f;   // each bonus imp drains this % of BASE mana on spawn
        // Warded Legion (wl): demon full-spell-resist chance per rank; rank 2 = Fear/Charm/Poly immunity.
        float  WardedLegionResistPct[2] = { 0.09f, 0.18f };
        // Vital Conduit (vc, REDESIGNED): Life Tap also heals your commanded demons for this
        // fraction of the health sacrificed, split among them (overheal allowed).
        float  VitalConduitHealPct[2] = { 0.50f, 1.00f };
        // Fel Conduit (fcd): demon attacks proc a Conduit stack (instant/free Shadow Bolt) per rank.
        float  FelConduitProcPct[2] = { 0.05f, 0.10f };
        // Grim Bargain (gb): demon<->owner damage proc synergy — buff amount per rank + proc chance.
        float  GrimBargainDamagePct[2] = { 0.06f, 0.12f };
        float  GrimBargainProcPct      = 0.15f;
        // Fel Blood (fb): Lash of Pain damage bonus per rank (Corruption-refresh half has no number).
        float  FelBloodLashPct[2] = { 0.15f, 0.30f };
        // Riftwalker (rw): on Demonic Circle: Teleport, warp demons to you + this move-speed burst.
        int32  RiftwalkerMoveSpeedPct = 30;
        uint32 RiftwalkerDurationMs   = 6000;
        // Fervent Standard (fs): your Demonic Circle is the legion's banner. Within Radius yds of it,
        // you and your demons deal +DamagePct; your demons take -MitigationPct. All three are direct
        // range checks against the owner's Demonic Circle GameObject (no aura bookkeeping).
        float  FerventStandardRadius        = 20.0f;
        float  FerventStandardDamagePct[2]     = { 0.04f, 0.08f };  // owner + demon offense
        float  FerventStandardMitigationPct[2] = { 0.05f, 0.10f };  // demon-only damage taken

        // Anchor Imp pet buff — SPEC-AGNOSTIC (all warlock specs, not a talent). The Imp lacks the
        // CC/tank utility of Succubus/Voidwalker, so its Firebolt damage is brought up toward
        // Felhunter tier. Targets ONLY the anchor Imp PET (entry 416) — NOT Wild Imps / Imp
        // legionnaires (those cast 290900 and are guardians, so WildImp.SPCoefficient is untouched).
        float  ImpAnchorDamageBonusPct = 0.30f;

        // Per-type legionnaire SIGNATURES — a periodic instant ability that gives each melee demon
        // type a damage identity (instead of being a plain-melee clone). SP coefficient + cooldown
        // (ms) each. Imp keeps its Firebolt (WildImp path); Voidwalker stays a pure-melee off-tank.
        float  FelguardCleaveSPCoef      = 0.15f;   // cleaves up to 3 targets (per-target SP)
        uint32 FelguardCleaveCooldownMs  = 6000;
        float  FelhunterShadowBiteSPCoef = 0.35f;   // single-target shadow nuke
        uint32 FelhunterShadowBiteCooldownMs = 6000;
        float  SuccubusLashSPCoef        = 0.25f;   // single-target shadow nuke
        uint32 SuccubusLashCooldownMs    = 5000;

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
        float BeaconDemonDamagePct           = 0.50f;

        // --- Phase 1 talents ---
        // Pactbound Fury (pf, T3) — demon crit chance per rank. Applied as a REAL crit:
        // melee via the core melee-outcome roll, spell via the demon's base spell crit
        // (PetScaling). The crit damage multiplier is the core's (2x melee / 1.5x spell).
        float PactboundFuryCritChancePct[3] = { 0.02f, 0.04f, 0.06f };  // per rank

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

        // --- Command Demon (Phase 3, §5) ---
        uint32 CommandDemonCooldownMs       = 45000;
        uint32 CommandDemonShardCost        = 1;
        // Dark Command (dc): CD reduction per rank + a short haste buff on responders.
        uint32 DarkCommandCdReductionMsPerRank = 5000;   // -5/10/15s at ranks 1/2/3
        float  DarkCommandHastePct          = 0.10f;     // responder attack-speed buff (fraction)
        uint32 DarkCommandHasteDurationMs   = 6000;
        // Enthralling Presence (Succubus signature): 8yd around the anchor's target; the
        // debuff (290501) is applied per enemy; the threat drop is imperative here.
        float  EnthrallingPresenceRadius    = 8.0f;
        int32  EnthrallingThreatDropPct     = 35;        // reduce owner+demon threat by this %
        // Voidwalker absorbs (fraction of max health): owner shield (signature) + self (echo).
        float  VoidwalkerOwnerAbsorbPctOfHp = 0.15f;
        float  VoidwalkerSelfAbsorbPctOfHp  = 0.10f;
        // Voidwalkers off-tank: they generate this multiple of threat from all damage (so what
        // they hit sticks to them and their shield matters). Command echo also taunts (§ changed).
        float  VoidwalkerThreatMultiplier   = 4.0f;
        // Echo / response SP scaling (added live at cast, like Firebolt / Doom Bolt).
        float  EmpoweredLashSPCoefficient   = 0.30f;
        float  InfernalCommandPulseSPCoefficient = 0.40f;

        bool  DebugLogShardIncome        = false;
    };

    // Global, refreshed on OnAfterConfigLoad.
    extern ModuleConfig gConfig;
}

#endif // MOD_DEMONOLOGY_REWORK_CONFIG_H
