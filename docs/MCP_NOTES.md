# SpellForge notes (the "MCP")

The spell/talent pipeline is **SpellForge** (`WOW-AC-3.3.5a/mcp-acore-spellforge`)
driving the content project `WOW-AC-3.3.5a/spellforge-content`. It is a
**deterministic, declarative** YAML→artifacts compiler, not a live black box —
so the Phase 0 "capability probe" collapses into documentation.

## Pipeline

```
data/spellforge/**.yaml           (this module — source of truth)
        │  build.sh links into spellforge-content/content/demonology/
        ▼
spellforge-content/content/  ──sf build──▶  spellforge-content/build/
                                              ├── dbc/*.dbc
                                              ├── sql/db-world/*.sql
                                              ├── mpq/patch-6.MPQ
                                              └── manifest.json  (sha256 ledger)
        │  build.sh vendors build/ → this module's dist/
        ▼
dist/  ──install.sh──▶  server data/dbc + world/char DBs + client Data/
```

## Capability findings (resolved by inspection, not probe — PLAN §1 Task 2)

| Question | Finding |
|---|---|
| **Idempotency** | Deterministic build; `manifest.json` sha256-hashes every artifact. Re-deploying an unchanged build is a no-op copy. No delta-apply workaround needed. |
| **Rollback** | Revert YAML → `sf build` → `sf deploy`; or restore from `build/backups/<ts>/` (SpellForge backs up each deploy target automatically). |
| **Dry-run** | `sf diff_build`, and `sf deploy` is **dry-run by default** (`--apply` to execute). |

## CLI reference (from `spellforge/cli/main.py`)

`sf init · validate · build --targets dbc,sql · deploy --targets sql,dbc,mpq [--apply]
· doctor · generate-module · gm · query`

## Status (verified 2026-08-15)

- SpellForge **installed** (`mcp-acore-spellforge/.venv`); 355 tests pass; `sf doctor` = *ready*.
- `build.sh` runs clean end-to-end: validate → symlink `data/spellforge` → `content/demonology`
  → `sf build --targets dbc,sql,mpq` → vendor **exactly the manifest artifacts** into `dist/`.
- `install.sh --dry-run` produces the full, correct deploy plan (DBCs+backup → server,
  `patch-6.MPQ` → client, world+characters SQL from `dist/` and `data/sql`).

## Content discovery — RESOLVED (Phase 0 Task 4)

SpellForge's loader scans `content/spells/**/*.yaml` and `content/talents/**/*.yaml`
via `Path.rglob`, which **does not traverse directory symlinks**. So:
- A symlinked *directory* (`content/spells/demonology -> module/spells`) is NOT found.
- `build.sh` therefore makes a real `content/spells/demonology/` dir and symlinks each
  YAML *file* into it (symlinked files in a real dir ARE found). Confirmed: all 7 slice
  spells compiled; `Spell.dbc` went 50011 → 50018 records (+7).

Two authoring gotchas found while building the slice:
- **Time units:** the parser accepts only `s`/`ms` (bare number = seconds). `1m` fails —
  write `60s`.
- IDs are pinned in `data/spellforge/ids.yaml` and seeded into `content/ids.yaml` by
  `tools/seed_ids.py`; SpellForge honored all seven pins (290000–290900).

The two `sf doctor` warnings are benign: SOAP down (worldserver stopped) and a
`patch-V.mpq` client-priority note (`sf patch merge` reconciles if needed).
