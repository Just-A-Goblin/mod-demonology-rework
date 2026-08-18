# mod-demonology-rework

A Demonology warlock rework for AzerothCore 3.3.5a (Playerbot fork) — "Master of
the Legion." Command an anchor pet + mirroring legionnaires as one unit, fed by a
soul-shard economy and a custom 78-point talent tree.

**The full plan is [`PLAN_demonology_legion_module.md`](PLAN_demonology_legion_module.md).**

## Design principle: self-contained + archivable

Everything needed to deploy lives in this directory, even the parts that require
editing core. SpellForge is only a **build-time** dependency; the generated
artifacts are vendored into `dist/`, so an archive of this repo installs offline.

```
data/spellforge/  authoring YAML (source of truth)  ──build.sh──▶  dist/ (vendored artifacts)
src/              behavioral C++ (pool, demon AI, scaling)
core-patches/     versioned *.patch + idempotent apply/revert
dist/             generated DBC/SQL/MPQ + manifest (checked in)
addon/            display-only client addon
tools/            check_ids.py, validate_tree.py
docs/             ID_RANGES, MCP_NOTES, CORE_PATCHES, DESIGN
```

## Entry points

| Script | When | Needs SpellForge? | Does |
|---|---|---|---|
| `./build.sh` | authoring | **yes** | validate YAML → `sf build` → vendor artifacts into `dist/` |
| `./install.sh [--apply]` | deploy (server) | no | apply core patches, link module, deploy DBC→server + SQL→DBs (dry-run by default) |
| `./deploy_client.sh [--apply]` | deploy (client) | **yes** | merge our DBCs into the winning client patch (letters outrank numbers, so a plain patch is ignored) |

The client patch is a separate, SpellForge-driven step because on a modded 3.3.5
client our DBCs must be *merged into* the highest-priority patch in place — that
merge reads the target client's MPQ and so can't be pre-baked offline.

## Status

**Phase 0 vertical slice ✅ verified in-game.** On top of the skeleton (config
WorldScript + `.legion` GM namespace), the slice adds Soul Harvest 3/3, Summon
Wild Imps, and Demonic Empowerment — 7 spells authored through SpellForge (in
`Spell.dbc`, +7 records) at pinned IDs 290000–290900, a Wild Imp creature (600000),
and the C++ behavior (shard proc, summon, empowerment). Deployed to the live realm
and confirmed working on a test character. This proves the whole pipeline:
SpellForge authoring → DBC → server + merged client patch → static C++ module → live gameplay.

**Next:** Phase 1 — the Command Pool + mirroring demon AI (so demons follow what
the anchor fights) and the core patches (PLAN §3.2). Stat inheritance and the full
talent tree follow. See PLAN §12 for sequencing.

## Prerequisites (author machine)

- SpellForge installed (`../mcp-acore-spellforge/install.sh`) — provides the `sf` CLI.
- The `../spellforge-content` project (already present) — the pristine DBC base.
- Python 3 with PyYAML (for the validators).
