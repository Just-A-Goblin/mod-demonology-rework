/*
 * mod-demonology-rework — config loader (WorldScript).
 */
#include "DemonologyConfig.h"

#include "Config.h"
#include "ScriptMgr.h"

#include <cstdlib>
#include <sstream>
#include <string>

namespace Demonology
{
    ModuleConfig gConfig;
}

using namespace Demonology;

namespace
{
    // Parse a comma-separated float list ("0.08, 0.16, 0.24") into out[0..count).
    // Missing/blank leaves the caller's existing (default) values untouched, so a
    // short or absent list simply keeps code defaults for the unspecified ranks.
    void LoadFloatList(std::string const& key, float* out, uint8 count)
    {
        std::string const val = sConfigMgr->GetOption<std::string>(key, "");
        if (val.empty())
            return;

        std::stringstream ss(val);
        std::string tok;
        for (uint8 i = 0; i < count && std::getline(ss, tok, ','); ++i)
        {
            size_t const a = tok.find_first_not_of(" \t");
            if (a == std::string::npos)
                continue;                       // blank slot — keep the default
            size_t const b = tok.find_last_not_of(" \t");
            out[i] = float(std::atof(tok.substr(a, b - a + 1).c_str()));
        }
    }
}

class demonology_config_worldscript : public WorldScript
{
public:
    demonology_config_worldscript() : WorldScript("demonology_config_worldscript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        gConfig.Enable                       = sConfigMgr->GetOption<bool>("Demonology.Enable", true);
        gConfig.SoulHarvestChancePerRank     = sConfigMgr->GetOption<float>("Demonology.SoulHarvest.ChancePerRank", 0.04f);
        gConfig.SoulHarvestInternalCdMs      = sConfigMgr->GetOption<uint32>("Demonology.SoulHarvest.InternalCooldownMs", 1000);
        gConfig.CruelMasterProcChanceMult    = sConfigMgr->GetOption<float>("Demonology.CruelMaster.ProcChanceMult", 2.0f);
        gConfig.CruelMasterIcdMultOnCrit     = sConfigMgr->GetOption<float>("Demonology.CruelMaster.IcdMultOnCrit", 0.5f);
        gConfig.CruelMasterBaseCritChance    = sConfigMgr->GetOption<float>("Demonology.CruelMaster.BaseCritChance", 0.05f);
        LoadFloatList("Demonology.ViciousPact.MeleePct",         gConfig.VpMeleePct,       3);
        LoadFloatList("Demonology.ViciousPact.SpellPct",         gConfig.VpSpellPct,       3);
        LoadFloatList("Demonology.FelArmory.DamagePct",          gConfig.FaDamagePct,      3);
        LoadFloatList("Demonology.FelConditioning.HealthPct",    gConfig.FcHealthPct,      3);
        LoadFloatList("Demonology.CursedVitality.DemonHealthPct", gConfig.CvDemonHealthPct, 2);
        LoadFloatList("Demonology.SavageInstincts.HastePct",     gConfig.SiHastePct,       3);
        gConfig.WildImpCount                 = sConfigMgr->GetOption<uint32>("Demonology.WildImp.Count", 3);
        gConfig.WildImpDurationMs            = sConfigMgr->GetOption<uint32>("Demonology.WildImp.DurationMs", 20000);
        gConfig.WildImpSPCoefficient         = sConfigMgr->GetOption<float>("Demonology.WildImp.SPCoefficient", 0.28f);
        gConfig.WildImpShardCost             = sConfigMgr->GetOption<uint32>("Demonology.WildImp.ShardCost", 2);
        gConfig.ImprovedLegionWildImpShardReduction = sConfigMgr->GetOption<uint32>("Demonology.ImprovedLegion.WildImpShardReduction", 1);
        gConfig.InfernalDurationMs           = sConfigMgr->GetOption<uint32>("Demonology.Infernal.DurationMs", 60000);
        gConfig.DoomguardDurationMs          = sConfigMgr->GetOption<uint32>("Demonology.Doomguard.DurationMs", 60000);
        gConfig.DoomBoltSPCoefficient        = sConfigMgr->GetOption<float>("Demonology.Doomguard.DoomBoltSPCoefficient", 0.64f);
        gConfig.DoomBoltBaseDamage           = sConfigMgr->GetOption<uint32>("Demonology.Doomguard.DoomBoltBaseDamage", 850);
        gConfig.DoomBlastBaseDamage          = sConfigMgr->GetOption<uint32>("Demonology.Doomguard.DoomBlastBaseDamage", 300);
        gConfig.DoomBlastSPCoefficient       = sConfigMgr->GetOption<float>("Demonology.Doomguard.DoomBlastSPCoefficient", 0.50f);
        gConfig.InfernalScale                = sConfigMgr->GetOption<float>("Demonology.Infernal.Scale", 0.70f);
        gConfig.InfernalCommandSlots         = uint8(sConfigMgr->GetOption<uint32>("Demonology.Infernal.CommandSlots", 2));
        gConfig.DoomguardCommandSlots        = uint8(sConfigMgr->GetOption<uint32>("Demonology.Doomguard.CommandSlots", 3));
        gConfig.DemonicEmpowermentHaste      = sConfigMgr->GetOption<float>("Demonology.DemonicEmpowerment.Haste", 0.30f);
        gConfig.DemonicEmpowermentDamage     = sConfigMgr->GetOption<float>("Demonology.DemonicEmpowerment.Damage", 0.20f);
        gConfig.DemonicEmpowermentDurationMs = sConfigMgr->GetOption<uint32>("Demonology.DemonicEmpowerment.DurationMs", 12000);
        gConfig.PoolBaseLegionnaires         = uint8(sConfigMgr->GetOption<uint32>("Demonology.Pool.BaseLegionnaires", 0));
        gConfig.PoolMaxLegionnaires          = uint8(sConfigMgr->GetOption<uint32>("Demonology.Pool.MaxLegionnaires", 4));
        gConfig.SummonLegionnaireShardCost   = sConfigMgr->GetOption<uint32>("Demonology.SummonLegionnaire.ShardCost", 1);
        gConfig.InheritHealthPctOfOwner      = sConfigMgr->GetOption<float>("Demonology.Inherit.HealthPctOfOwner", 0.60f);
        gConfig.InheritMeleeDamagePerSP      = sConfigMgr->GetOption<float>("Demonology.Inherit.MeleeDamagePerSP", 0.15f);
        gConfig.LegionStandardHealthPctOfOwner = sConfigMgr->GetOption<float>("Demonology.LegionStandard.HealthPctOfOwner", 0.40f);
        gConfig.ThreatNonAnchorMultiplier    = sConfigMgr->GetOption<float>("Demonology.Threat.NonAnchorMultiplier", 0.50f);
        gConfig.LegionAuraDamagePct          = sConfigMgr->GetOption<float>("Demonology.LegionAura.DamagePct", 0.05f);
        gConfig.LegionAuraHastePct           = sConfigMgr->GetOption<float>("Demonology.LegionAura.HastePct", 0.05f);
        gConfig.BeaconDemonDamagePct         = sConfigMgr->GetOption<float>("Demonology.Beacon.DemonDamagePct", 0.50f);
        // --- Phase 1 talents ---
        LoadFloatList("Demonology.PactboundFury.CritChancePct", gConfig.PactboundFuryCritChancePct, 3);
        LoadFloatList("Demonology.DemonicRebirth.ChancePct", gConfig.DemonicRebirthChancePct, 2);
        gConfig.DemonicRebirthIcdMs          = sConfigMgr->GetOption<uint32>("Demonology.DemonicRebirth.IcdMs", 60000);
        LoadFloatList("Demonology.BoundByBlood.DamagePct", gConfig.BoundByBloodDamagePct, 2);
        LoadFloatList("Demonology.BoundByBlood.HastePct",  gConfig.BoundByBloodHastePct,  2);
        gConfig.BoundByBloodDurationMs       = sConfigMgr->GetOption<uint32>("Demonology.BoundByBlood.DurationMs", 10000);
        gConfig.BoundByBloodIcdMs            = sConfigMgr->GetOption<uint32>("Demonology.BoundByBlood.IcdMs", 180000);
        gConfig.BoundByBloodRefundShard      = sConfigMgr->GetOption<bool>("Demonology.BoundByBlood.RefundShard", true);
        LoadFloatList("Demonology.OverlordsPresence.HealthPct", gConfig.OverlordsPresenceHealthPct, 3);
        LoadFloatList("Demonology.OverlordsPresence.HastePct",  gConfig.OverlordsPresenceHastePct,  3);
        LoadFloatList("Demonology.CursedVitality.OwnerStaminaPct", gConfig.CursedVitalityOwnerStaminaPct, 2);
        gConfig.LegionAuraMinDemons          = sConfigMgr->GetOption<uint32>("Demonology.LegionAura.MinDemons", 1);
        // --- Command Demon (Phase 3) ---
        gConfig.CommandDemonCooldownMs       = sConfigMgr->GetOption<uint32>("Demonology.CommandDemon.CooldownMs", 45000);
        gConfig.CommandDemonShardCost        = sConfigMgr->GetOption<uint32>("Demonology.CommandDemon.ShardCost", 1);
        gConfig.DarkCommandCdReductionMsPerRank = sConfigMgr->GetOption<uint32>("Demonology.DarkCommand.CdReductionMsPerRank", 5000);
        gConfig.DarkCommandHastePct          = sConfigMgr->GetOption<float>("Demonology.DarkCommand.HastePct", 0.10f);
        gConfig.DarkCommandHasteDurationMs   = sConfigMgr->GetOption<uint32>("Demonology.DarkCommand.HasteDurationMs", 6000);
        gConfig.EnthrallingPresenceRadius    = sConfigMgr->GetOption<float>("Demonology.EnthrallingPresence.Radius", 8.0f);
        gConfig.EnthrallingThreatDropPct     = sConfigMgr->GetOption<int32>("Demonology.EnthrallingPresence.ThreatDropPct", 35);
        gConfig.VoidwalkerOwnerAbsorbPctOfHp = sConfigMgr->GetOption<float>("Demonology.Voidwalker.OwnerAbsorbPctOfHp", 0.15f);
        gConfig.VoidwalkerSelfAbsorbPctOfHp  = sConfigMgr->GetOption<float>("Demonology.Voidwalker.SelfAbsorbPctOfHp", 0.10f);
        gConfig.VoidwalkerThreatMultiplier   = sConfigMgr->GetOption<float>("Demonology.Voidwalker.ThreatMultiplier", 4.0f);
        gConfig.EmpoweredLashSPCoefficient   = sConfigMgr->GetOption<float>("Demonology.EmpoweredLash.SPCoefficient", 0.30f);
        gConfig.InfernalCommandPulseSPCoefficient = sConfigMgr->GetOption<float>("Demonology.InfernalCommandPulse.SPCoefficient", 0.40f);
        gConfig.DebugLogShardIncome          = sConfigMgr->GetOption<bool>("Demonology.Debug.LogShardIncome", false);
    }
};

void AddSC_demonology_config()
{
    new demonology_config_worldscript();
}
