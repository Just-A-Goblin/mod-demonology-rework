/*
 * mod-demonology-rework — config loader (WorldScript).
 */
#include "DemonologyConfig.h"

#include "Config.h"
#include "ScriptMgr.h"

namespace Demonology
{
    ModuleConfig gConfig;
}

using namespace Demonology;

class demonology_config_worldscript : public WorldScript
{
public:
    demonology_config_worldscript() : WorldScript("demonology_config_worldscript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        gConfig.Enable                       = sConfigMgr->GetOption<bool>("Demonology.Enable", true);
        gConfig.SoulHarvestChancePerRank     = sConfigMgr->GetOption<float>("Demonology.SoulHarvest.ChancePerRank", 0.04f);
        gConfig.SoulHarvestInternalCdMs      = sConfigMgr->GetOption<uint32>("Demonology.SoulHarvest.InternalCooldownMs", 1000);
        gConfig.CruelMasterCritMultiplier    = sConfigMgr->GetOption<float>("Demonology.CruelMaster.CritMultiplier", 2.0f);
        gConfig.WildImpCount                 = sConfigMgr->GetOption<uint32>("Demonology.WildImp.Count", 3);
        gConfig.WildImpDurationMs            = sConfigMgr->GetOption<uint32>("Demonology.WildImp.DurationMs", 20000);
        gConfig.WildImpSPCoefficient         = sConfigMgr->GetOption<float>("Demonology.WildImp.SPCoefficient", 0.28f);
        gConfig.InfernalDurationMs           = sConfigMgr->GetOption<uint32>("Demonology.Infernal.DurationMs", 60000);
        gConfig.DoomguardDurationMs          = sConfigMgr->GetOption<uint32>("Demonology.Doomguard.DurationMs", 60000);
        gConfig.DoomBoltSPCoefficient        = sConfigMgr->GetOption<float>("Demonology.Doomguard.DoomBoltSPCoefficient", 0.50f);
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
        gConfig.DebugLogShardIncome          = sConfigMgr->GetOption<bool>("Demonology.Debug.LogShardIncome", false);
    }
};

void AddSC_demonology_config()
{
    new demonology_config_worldscript();
}
