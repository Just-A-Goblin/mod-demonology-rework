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

// Phase 5 — Doombrand capstone
void AddSC_demonology_doombrand();

// Phase 6 — Riftwalker
void AddSC_demonology_riftwalker();

// Dead-node redesigns — Vital Conduit (Life Tap heals the legion)
void AddSC_demonology_vital_conduit();

// Dead-node redesigns — Fel Corruption (Corruption ticks feed the economy)
void AddSC_demonology_fel_corruption();

// Dead-node redesigns — Fervent Standard (Demonic Circle = legion banner)
void AddSC_demonology_fervent_standard();

void Addmod_demonology_reworkScripts()
{
    AddSC_demonology_config();
    AddSC_legion_commandscript();

    AddSC_demonology_shard_economy();
    AddSC_demonology_summon_spells();
    AddSC_demonology_empowerment_spells();

    AddSC_demonology_command_pool();

    AddSC_demonology_command_demon();

    AddSC_demonology_doombrand();

    AddSC_demonology_riftwalker();

    AddSC_demonology_vital_conduit();

    AddSC_demonology_fel_corruption();

    AddSC_demonology_fervent_standard();
}
