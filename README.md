# mod-demonology-rework

A Demonology warlock rework for AzerothCore 3.3.5a (Playerbot fork) — "Master of
the Legion." Command an anchor pet + mirroring legionnaires as one unit, fed by a
soul-shard economy and a custom 78-point talent tree.

**Governing design:** [`DEMONOLOGY_DESIGN_V2.md`](DEMONOLOGY_DESIGN_V2.md) · **current-state audit:** [`DEMONOLOGY_STATUS.md`](DEMONOLOGY_STATUS.md) · **balance:** `docs/balance_model.py`.

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

## Build & deploy — full workflow

Three kinds of change need different steps. Do the ones your change touched, in order.

**1. Author content** (talents, spells, icons, tooltips, spell costs, tree layout — anything in
`data/spellforge/`):
```
./build.sh                       # validate YAML → sf build → vendor DBC/SQL/MPQ into dist/
```

**2. Compile the C++ module** (anything in `src/` — pool, AI, scaling, scripts):
```
cd /home/leo/wow/build
cmake .                          # ONLY when a NEW .cpp was added (re-globs sources); skip otherwise
make -j$(nproc) && make install  # builds + installs the worldserver
```

**3. Deploy to server + client:**
```
./install.sh --apply             # core patches, link module, DBCs→server, SQL→world/characters, seed live .conf, addon
./deploy_client.sh --apply       # rebuild the MERGED client patch (patch-V.mpq) and copy to the client
```
- `install.sh` verifies/backs up before copying. It does **not** compile — do step 2 first for C++ changes.
- Hand-written base SQL in `data/sql/db-world/base/*.sql` (script bindings, trainer rows) is applied by
  `install.sh` and also auto-applied by the server's DB updater on boot.

**4. Restart / reload:**
```
# find + SIGINT the running worldserver, wait for port 8085 to free, relaunch. The live server runs from
#   /home/leo/WOW-BACKUP-8-11-26/wow/server/bin/worldserver  (the /home/leo/wow symlink resolves here);
# find it by its listening port, NOT a process-name grep:
#   SRVPID=$( (ss -ltnp||netstat -ltnp) | grep ':8085' | grep -oE 'pid=[0-9]+' | head -1 | cut -d= -f2)
```
- **DBC / SQL / C++ changes require a worldserver restart** (the server caches DBC + SpellInfo at boot).
- **conf-only tweaks** (a value in the live `.conf`) need only `.reload config` in the server console — no
  restart, no rebuild.
- After a client-patch deploy, **fully relaunch the WoW client** (not `/reload`) to load the new `patch-V.mpq`.

> **Gotcha (talent-tab edits):** any Talent.dbc change that removes/renames a warlock-tab talent must clear
> orphaned `character_talent` rows before boot or the server crash-loops on load. Simply *moving* a talent
> (same rank spells) is safe. See the talent-tree memory / `DEMONOLOGY_STATUS.md`.

## Status

**COMPLETE & LIVE (2026-08-20).** All 36 talent nodes, both baseline abilities (Summon Wild Imps,
Demonic Empowerment), the Command Demon + Doombrand capstones, the shard economy, stat inheritance,
mirroring demon AI, and the full Command Pool are built, deployed, and playtested. The three original
dead nodes were redesigned (Fel Corruption, Vital Conduit, Fervent Standard), the two partials finished
(Improved Legion, Savage Instincts), the tree reshuffled, and a balance pass (FLATTEN) applied. Baseline
abilities are trainer-taught (Summon Wild Imps @10, Demonic Empowerment @50); the rest is trainer/talent.

**Remaining work is validation only:** replace the ±20% expected-value balance model
(`docs/balance_model.py`) with real level-60/80 combat parses. See `DEMONOLOGY_STATUS.md` for the live
node audit and the balance-model notes for tuning history.

## Prerequisites (author machine)

- SpellForge installed (`../mcp-acore-spellforge/install.sh`) — provides the `sf` CLI.
- The `../spellforge-content` project (already present) — the pristine DBC base.
- Python 3 with PyYAML (for the validators).
