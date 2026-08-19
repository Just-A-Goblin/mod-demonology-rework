/*
 * mod-demonology-rework — script loader.
 *
 * AzerothCore discovers this module by the function name below:
 * Add<module-dir-with-underscores>Scripts(). It aggregates the per-feature
 * AddSC_* registrars. Add new features by declaring their AddSC_* here and
 * calling it — nothing else wires them in.
 *
 * Phase 0 skeleton: config + the `.legion` GM command namespace only.
 * Phase 1+ registrars (pool, demon AI, shard economy, ...) get added as they land.
 */

// Phase 0
void AddSC_demonology_config();
void AddSC_legion_commandscript();

// Vertical slice (Phase 0 Task 4)
void AddSC_demonology_shard_economy();
void AddSC_demonology_summon_spells();
void AddSC_demonology_empowerment_spells();

// Phase 1
void AddSC_demonology_command_pool();

// Phase 3 — Command Demon
void AddSC_demonology_command_demon();

void Addmod_demonology_reworkScripts()
{
    AddSC_demonology_config();
    AddSC_legion_commandscript();

    AddSC_demonology_shard_economy();
    AddSC_demonology_summon_spells();
    AddSC_demonology_empowerment_spells();

    AddSC_demonology_command_pool();

    AddSC_demonology_command_demon();
}
