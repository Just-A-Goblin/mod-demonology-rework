# Core patches

Every core touch point ships here as a versioned `*.patch` applied by `apply.sh`
(reversed by `revert.sh`). **One entry per patch below.** Fork discipline (PLAN
§0.1): patch core to add *seams*, keep *logic* in the module. If any entry is
empty, the patch does not exist yet.

Target tree: AzerothCore **Playerbot** branch at `AC_SOURCE`
(default `/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore`).

## How to author a patch

1. Make the minimal change in `$AC_SOURCE` (a hook, a `virtual`, relaxing a
   hardcoded single-minion assumption).
2. `cd $AC_SOURCE && git diff -- src/path/to/File.cpp > \
   modules/.../core-patches/NN-short-name.patch` (or `git diff` piped to the file).
3. Add an entry below and a smoke test so a silently-dropped hook fails loudly.

## Patch registry

| Patch | File / function | Why | Breaks if dropped |
|---|---|---|---|
| `01-unit-base-spell-crit-setter.patch` | `Unit::SetBaseSpellCritChance` / `GetBaseSpellCritChance` (Unit.h) | expose the protected `m_baseSpellCritChance` seam so the module can grant owned demons (plain creatures, base 5%) real, visible spell crits — used by Pactbound Fury in `PetScaling::ApplyInheritance` | build fails (unresolved `SetBaseSpellCritChance`); demon spell crits would otherwise be stuck at the flat 5% base with no Pactbound Fury contribution |

Planned seams (from PLAN §3.2 and §8), to be filled as each phase lands:

| Patch | File / function | Why | Breaks if dropped |
|---|---|---|---|
| _tbd_ | `Unit::SetMinion` | permit multiple owned guardians without disturbing `UNIT_FIELD_SUMMON` / anchor pet GUID | new legionnaires unsummon each other |
| _tbd_ | `Player::GetPet()` callers | several talent/threat paths assume the only owned unit | miscounts, wrong threat/scaling |
| _tbd_ | `Guardian::UpdateAttackPowerAndDamage()` / `UpdateStats()` (`StatSystem.cpp`) | seam for `PetScaling::ApplyInheritance` | pack is flat, gear-independent, worthless at 80 |
