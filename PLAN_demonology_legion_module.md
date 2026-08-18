# Implementation Plan — `mod-demonology-rework` (v4)

**Target:** AzerothCore (3.3.5, **Playerbot fork** at `WOW-BACKUP-8-11-26/wow/azerothcore`) + owner-controlled 3.3.5a client at `C:\GAMES\WoW-3.3.5a`
**Environment:** single local realm, single player, backed up. No PTR — the working realm is the test realm.
**Spell/talent pipeline:** SpellForge (`WOW-AC-3.3.5a/mcp-acore-spellforge`) + its content project (`WOW-AC-3.3.5a/spellforge-content`).
**Source design:** `demonology_master_of_the_legion.md`
**Audience:** Claude Code, working in the module tree at `WOW-AC-3.3.5a/mod-demonology-rework`

> **v3 changes:** demon army reverted to **anchor + mirroring legionnaires** (commanded as one unit); addon is now display-only; ID ranges and MCP idempotency are unknown, so both become hard Phase 0 probes; deployment/migration ceremony stripped for a solo local realm.
>
> **v4 changes (post-environment audit):**
> - Module renamed `mod-demonology-legion` → **`mod-demonology-rework`** (matches the on-disk dir).
> - The "MCP" is **SpellForge** — a deterministic, declarative YAML→DBC/SQL/MPQ/C++ pipeline, not a live black box. Idempotency/rollback/dry-run are **resolved by inspection** (see §0.2, §1 Task 2); the Phase 0 "probe" is now just documentation.
> - **ID collision audit done:** custom space is empty. IDs **reassigned into SpellForge's configured ranges** (spell `200000–299999`, talent `5000–9999`); the old `300000`/`3000` proposals are dropped. See §1 Task 1.
> - **Self-contained, archivable deployment:** the module owns the authoring YAML and *vendors the generated artifacts* (`dist/`), so an archive of this directory installs with no SpellForge present. Two entry points: `build.sh` (author-time, needs SpellForge) and `install.sh` (offline). See §2, §10.
> - **Core patches** ship as versioned `.patch` files + idempotent `apply.sh`/`revert.sh` under `core-patches/`.

---

## 0. Confirmed constraints

| Constraint | Status | Consequence |
|---|---|---|
| Core fork | **Allowed** (Playerbot branch) | Minion handling and stat inheritance done properly in core. Fork discipline in §0.1. Patches ship as `core-patches/*.patch` + apply/revert scripts. |
| Spell/talent authoring | **SpellForge** — deterministic YAML→DBC/SQL/MPQ/C++ pipeline | No hand DBC editing. Iteration on spells is cheap and reversible (revert YAML, rebuild). |
| Existing custom ID ranges | **Audited — empty.** | IDs assigned inside SpellForge ranges; see §1 Task 1. |
| MCP idempotency / rollback | **Resolved by inspection.** | Declarative + deterministic (`manifest.json` hashes); `sf diff_build` = dry-run; `sf deploy` auto-backs-up. Not a one-way apply. |
| Conflicting modules | **None** | |
| Custom addon | **Allowed** | Used for demon **display only** — see §3.3. |
| Test realm | **None — local realm is the realm.** Backup exists. | No migration ceremony, no announcements, no refunds. Iterate directly. |

### 0.1 Fork discipline

A fork that accumulates feature logic in core is unmergeable within two upstream releases. So:

**Patch core to add seams. Keep logic in the module.**

Acceptable core changes: a new `ScriptMgr` hook, a `virtual` where there was none, relaxing a hardcoded single-minion assumption. Not acceptable: `if (owner->HasSpell(SOUL_HARVEST_R3))` inside `StatSystem.cpp`.

Required:
- Dedicated branch, **rebased** onto upstream, so the patch series stays legible.
- `docs/CORE_PATCHES.md` — one entry per touch point: file, function, why, and what breaks if a merge silently drops it.
- A smoke test per core patch, so a dropped hook fails loudly instead of the spec quietly ceasing to scale.

### 0.2 What SpellForge is, and the source-of-truth split

SpellForge compiles a git-trackable YAML project into DBCs, world-DB SQL, a client MPQ, and a C++ script-module scaffold. On this machine:
- `spellforge-content/base/` — a **pristine snapshot of the server's real DBCs**; the compile base, never a deploy target. It is server-specific and stays in `spellforge-content`.
- `sf build` → `spellforge-content/build/{dbc,sql/db-world,mpq,manifest.json}`. `manifest.json` sha256-hashes every artifact — the idempotency ledger.
- `sf deploy` copies DBCs → server `data/dbc`, runs SQL → world/characters DBs, copies `patch-6.MPQ` → client `Data/`, backing each target up to `build/backups/<ts>/` first.

Authoring is cheap and reversible, so:
- **Author freely.** If something reads better as a real spell with a real tooltip, make it one.
- Keep internal markers (pool-membership tags, threat auras, ICD trackers) out of the visible spellbook — for combat-log and tooltip cleanliness.
- **The demon YAML is this module's source of truth** and lives in `data/spellforge/` (namespace `demonology.*`). `build.sh` links it into `spellforge-content/content/demonology/` and runs `sf build`. SpellForge is the *applier*, not the owner of the content.

What SpellForge does **not** manage (and the module ships directly): `creature_template` / `pet_levelstats` rows for new demons, the behavioral C++, and the core patches.

---

## 1. Phase 0 — Foundations

**Task 1 — ID collision audit — DONE.** `spellforge-content/content/ids.yaml` holds the whole custom registry; only a retired `druid.ice_lance` (200000) and a stale `mage.test_bolt` example occupy space. Assignments below sit inside SpellForge's configured ranges (`spellforge.toml [ranges]`) and are recorded in `docs/ID_RANGES.md`:

| Asset | Range | SpellForge range | Notes |
|---|---|---|---|
| Player-facing spells | `290000–290499` | spell `200000–299999` | |
| Internal/hidden spells | `290500–290899` | spell `200000–299999` | pool tags, threat auras, ICD markers |
| Pet ability spells | `290900–291199` | spell `200000–299999` | Firebolt, Doom Bolt, etc. |
| Talent entries | `9000–9099` | talent `5000–9999` | |
| Talent **tab** | *reuse existing Demonology tab* | (talent_tab `500–999` is for new tabs) | replace in place; read real `TalentTab.dbc` id |
| Spell icons (custom) | `19000–19099` | spell_icon `10000–19999` | only if hand-authored icons |
| Creature entries | `600000–600099` | *(world DB, not SpellForge)* | ships in `data/sql/` |
| Soul Shard reagent | `6265` (existing) | — | no new item |

`tools/check_ids.py` fails the build on out-of-range or colliding IDs (cross-checks `data/spellforge/**` against these ranges and against `spellforge-content/content/ids.yaml`).

**Task 2 — SpellForge capability notes (resolved by inspection, not a probe).** Record in `docs/MCP_NOTES.md`:
1. **Idempotency** — builds are deterministic; `manifest.json` sha256-hashes artifacts; re-`deploy` of an unchanged build is a no-op copy. ✓
2. **Rollback** — revert YAML → `sf build` → `sf deploy`; or restore from `build/backups/<ts>/`. ✓
3. **Dry-run** — `sf diff_build` and `sf deploy --dry-run`. ✓

No delta-apply workaround needed; `build.sh` drives `sf build` straight from the module's YAML.

**Task 3 — Skeletons.** Module and addon both building and loading, plus a `.legion` GM command namespace (`.legion pool`, `.legion summon <entry>`, `.legion shards`, `.legion dumpstats`) — on a solo realm these are the primary debugging surface and will pay for themselves within a day.

**Task 4 — Vertical slice. ✅ VERIFIED IN-GAME (2026-08-15).** Soul Harvest 3/3 + Summon Wild Imps + Demonic Empowerment authored through SpellForge (7 spells, pinned IDs 290000–290900, `Spell.dbc` +7 records), Wild Imp creature 600000, and C++ behavior (shard proc / summon / empowerment) — deployed to the live realm (module linked, worldserver rebuilt static, DBC+SQL applied, client patch-V re-merged) and confirmed working on test char Legiontest. Proves the full pipeline end to end. Findings: SpellForge scans `content/{spells,talents}/**` and `rglob` skips dir symlinks (→ per-file symlinks in `build.sh`); time units `s`/`ms` only; talent-tab replacement deferred to Phase 5 (Soul Harvest ships as a learnable passive). In-game bug fixes: `PSendSysMessage` uses `{}` not `%`; buff on a `unit_caster`-effect spell needs `AddAura` not `CastSpell`; `REACT_DEFENSIVE` for commanded guardians; non-`passive` buff for a visible icon; modern `creature_template` has no `spell1`. See `docs/VERIFY_SLICE.md`, `docs/CORE_PATCHES.md`.

---

## 2. Repository layout

The module is **self-contained and archivable**: authoring YAML + hand-written C++ + core patches + *vendored generated artifacts*. `build.sh` needs SpellForge; `install.sh` does not.

```
mod-demonology-rework/
├── build.sh                          # AUTHOR-TIME: link data/spellforge → spellforge-content, sf build, vendor artifacts → dist/
├── install.sh                        # OFFLINE INSTALL: apply patches, link module, deploy dist/ + data/sql
├── CMakeLists.txt
├── conf/mod_demonology_rework.conf.dist
├── data/
│   ├── spellforge/                   # SOURCE OF TRUTH: demon spell/talent YAML (namespace demonology.*)
│   │   ├── spells/                   #   spells, costs, cooldowns, coefficients
│   │   ├── talents/                  #   36-node tree: tier, column, ranks, prereqs
│   │   └── ids.yaml                  #   key -> ID map for the demonology.* namespace
│   └── sql/                          # world/char rows SpellForge does NOT own
│       ├── db-world/                 #   creature_template, pet_levelstats (600000-600099)
│       └── db-characters/            #   character_legion_slots, talent-reset migration
├── dist/                             # VENDORED generated artifacts (checked in — makes install offline)
│   ├── dbc/                          #   patched *.dbc from sf build
│   ├── sql/db-world/                 #   spell_dbc, spell_ranks, spell_proc, spell_script_names, ...
│   ├── mpq/patch-6.MPQ               #   client patch
│   └── manifest.json                 #   sha256 ledger copied from build/
├── core-patches/
│   ├── *.patch                       # versioned diffs vs the AC Playerbot branch
│   ├── apply.sh                      # idempotent (git apply --check first; skip if applied)
│   ├── revert.sh
│   └── CORE_PATCHES.md               # one entry per touch point: file, function, why, breakage
├── tools/
│   ├── validate_tree.py              # YAML -> assertions vs. design doc; runs BEFORE build
│   └── check_ids.py                  # enforce ID ranges + cross-check spellforge-content/ids.yaml
├── addon/MasterOfTheLegion/          # DISPLAY ONLY
│   ├── MasterOfTheLegion.toc
│   └── LegionFrames.lua / .xml       # health + buff frames for slots 1-3
├── docs/
│   ├── DESIGN.md                     # frozen copy of the design doc
│   ├── CORE_PATCHES.md               # (canonical copy; core-patches/CORE_PATCHES.md may symlink)
│   ├── ID_RANGES.md
│   └── MCP_NOTES.md                  # SpellForge idempotency/rollback/dry-run findings
└── src/
    ├── mod_demonology_rework_loader.cpp
    ├── CommandPool.h/.cpp            # slots, persistence, OnPoolChanged
    ├── DemonAI.h/.cpp                # mirroring AI
    ├── LegionCommand.cpp             # .legion GM command namespace
    ├── ShardEconomy.cpp
    ├── SummonSpells.cpp
    ├── EmpowermentSpells.cpp
    ├── LegionStandard.cpp
    ├── Metamorphosis.cpp
    ├── TalentEffects.cpp
    ├── PetScaling.cpp
    └── Util.cpp
```

---

## 3. Phase 1 — The Command Pool

### 3.1 Architecture: anchor + mirroring legionnaires

The army is commanded **as a single unit**. This matches the design's "general, not a micro-manager" fantasy and sidesteps the client's one-pet-frame limit rather than fighting it.

- **Slot 0 — the Anchor.** A real `Pet` (Imp / Voidwalker / Succubus / Felhunter / Felguard). Stock pet frame, stock action bar, stock persistence. Untouched core path — this is what keeps normal warlock behaviour intact for other specs.
- **Slots 1–3 — Legionnaires.** `Guardian` minions in `Unit::m_Controlled` running `DemonAI`, which **mirrors the anchor's target and react state**. Attack what the anchor attacks; follow when it follows; respond as a group to Demonic Empowerment and Command Demon.
  - **React-state mirroring:** legionnaires inherit the anchor's stance — anchor passive → all passive, aggressive → all aggressive, etc. *(Increment 1 hardcodes `REACT_DEFENSIVE`; mirroring the stance is the next refinement.)*
  - **CC-awareness:** legionnaires must **never attack a target the anchor has crowd-controlled** (Succubus **Seduce** especially; also Fear/Banish), so the pack doesn't break the anchor's CC. A Succubus anchor can Seduce one target while the legion cleaves the rest. If the only target is the anchor's CC'd one, idle/follow instead.
  - **Formation spacing:** legionnaires must **not stack on one point** — the pack has to be countable at a glance (matters for allied and hostile players alike). Give each a slot-based follow angle (fan out) and a spread spawn offset, not the shared default follow angle.
  - **Follow the anchor, not the owner:** idle legionnaires follow the pet, so pet **Stay** holds the legion with it and pet **Follow** trails them near the owner.
  - **Emergent command channel (preserve):** with the pet aggressive/defensive, **right-clicking a mob** makes the owner attack it, and the mirror's `owner->GetVictim()` fallback sends the legion onto that mob — *without* breaking the anchor's Seduce (CC-awareness only excludes the anchor's own CC'd target). This is the direct "send the legion here" order that leaves the anchor's focus intact.

The player's pet bar effectively becomes the army's command bar. Attack orders, passive/defensive, and stay/follow all propagate through the anchor.

> **Deferred upgrade path:** per-demon control is possible later via an addon command channel (`CHAT_MSG_ADDON`, prefix `MOTL`). If it's ever added, it is a client input path and needs server-side ownership checks, ability validation, and rate limiting — a modified client must not be able to bypass demon cooldowns. Not built now; noted so the decision isn't relitigated from scratch.

### 3.2 Core patches required

Audit and relax, documenting each in `CORE_PATCHES.md`:
- `Unit::SetMinion` — the path that unsummons an existing minion when a new controllable one is set. Must permit multiple owned guardians without disturbing `UNIT_FIELD_SUMMON` or the anchor's pet GUID.
- `Player::GetPet()` callers — grep hard and verify none assume "the only owned unit." Several talent and threat paths do.
- `m_petStable` / `PET_SAVE_*` — leave alone. Legionnaire persistence is ours (§3.4).

### 3.3 Addon — display only

Health bars, buff icons (Demonic Empowerment, Feed the Pit stacks), and duration timers for slots 1–3. **Reads client-visible state only; sends nothing to the server.** No trust boundary, no rate limiting, no validation layer.

Buff-stack visibility matters more than it sounds: Feed the Pit stacking to 3 and Demonic Empowerment uptime are the spec's two core feedback loops, and neither is visible on a stock interface for demons 2–4.

### 3.4 Pool management

`CommandPool`, one per player, built on login, torn down on logout:
- `uint8 GetMaxSlots(Player*)` — 1 base, +1 each for Expanded Command, Expanded Command II, Legion Commander.
- `TrySummon(...)` — if permanent and full, unsummon the **oldest** occupant (design §2).
- Temporary demons (Wild Imps, rift spawns, Grimoire duplicates, Felhound Pack) never touch the pool. Tag with a marker aura so every downstream system separates permanent from temporary with one `HasAura` check.
- `OnPoolChanged()` — **single callback** that everything pool-size-dependent routes through: Legion Aura, Soul Link scaling, Grand Warlock's Design's +4%/demon, threat auras. One source of truth so they can't disagree about the count.
- Threat: non-anchor demons get 50% reduced threat in a group (design §2). Hidden `SPELL_AURA_MOD_THREAT`, refreshed on group change. Low priority on a solo realm — implement, don't tune.

```sql
CREATE TABLE `character_legion_slots` (
  `guid` INT UNSIGNED NOT NULL,
  `slot` TINYINT UNSIGNED NOT NULL,
  `creature_entry` INT UNSIGNED NOT NULL,
  `saved_health_pct` FLOAT NOT NULL DEFAULT 1,
  PRIMARY KEY (`guid`,`slot`)
);
```
Restore on login via a short delayed event — summoning during the login sequence is fragile. Clear on talent reset if max slots drops.

### 3.5 Exit criteria

Four demons attacking the anchor's target, surviving logout/login, cleaning up on disconnect, with zero behaviour change to a Destruction warlock's pet.

---

## 4. Phase 2 — The shard economy

### 4.1 Soul Harvest (T1)
- Trigger on damage dealt by a player-owned legion unit. Use `UnitScript::OnDamage` if it carries what's needed; otherwise add a proper hook rather than scattering checks in core.
- `4/8/12%` per rank, **per-player** 1000ms internal cooldown (config-driven) held in `CommandPool` — not per demon. This is the economy's throttle and the design is explicit about it.
- On proc: `StoreNewItem` for item **6265** through the normal `CanStoreNewItem` path, so soul-bag rules and full-bag failure stay core's problem. **If storage fails, do not consume the ICD** — drop silently, let it re-roll.

### 4.2 Cruel Master (T2)
Same handler: crits roll at double rate, ICD shortened `0.25/0.5s`. Needs the crit flag at the hook — if unavailable, that's a hook-signature patch, not a workaround.

### 4.3 Drain Soul suppression
Design §2.1: no shards from Drain Soul for this spec, **without touching the baseline spell**. `SpellScript` on the shard-creation effect, suppressing only when the caster has any rank of Soul Harvest.

### 4.4 Bound by Blood (T10)
On legion demon death: refund 1 shard, apply `15/30%` damage / `25/45%` haste for 10s to survivors. Guard against resummon/death loops on high-churn AoE.

### 4.5 Exit criteria
Income over a 5-minute dummy fight with a 3-demon pack lands within ±20% of the design's "one shard per 6–8 seconds."

---

## 5. Phase 3 — Summoning spells

| Spell | Notes |
|---|---|
| **Summon Wild Imps** | 3 imps, 20s, Firebolt 2.0s base cast. Wrath of the Legion chains extra imps — enforce "no more than twice from a single cast" with a **generation counter on the summon**, not a global cap. |
| **Call Felhound Pack** | 2 Felhunters, 30s. Melee dispels one beneficial effect, heals other legion demons 3% max HP. |
| **Grimoire of Synergy** | Duplicates the anchor with a **stat snapshot at cast time** (design §9.2 — inherits active procs and trinket buffs). Snapshot on spawn; do not link live. Separate code path from §8; don't unify by accident. |
| **Rend Veil** | Channel, persistent invisible spawner, one random lesser demon every 3s for 20s, 15s lifespans. **Haste must not affect the 3s interval.** Ignores the pool. |
| **Doomguard / Infernal** | Reuse baseline. Eternal Servitude converts them to permanent pool occupants at 60s CD via a scripted cast override, not duplicate spells. |
| **Summon Felguard** | Talent-gated (T7). Gate on talent at cast **and** re-verify on login so a respec strips it. |

Shard costs ride the reagent path (item 6265). Improved Legion rank 2 reduces cost by 1 (min 1) — one `SpellScript` across the summons, not duplicated spell rows.

---

## 6. Phase 4 — Empowerment and command spells

- **Demonic Empowerment** — +30% haste, +20% damage, CC immunity, 12s, all legion demons. Write the **target selector once and parameterize it** — Supreme Empowerment (T10) is exactly the switch that extends it to temporary demons.
- **Feed the Pit** — 40% max-HP heal (~40% SP), +15% damage and size per stack, 3 stacks, 20s. `SetObjectScale`; verify scale unwinds correctly on partial stack expiry.
- **Hand of Gul'dan** — ground-targeted, +25% demon crit 10s, trivial damage (~12% SP). Shadowflame Legion adds a `15/30%`-of-SP absorb, 3 stacks.
- **Command Demon** — all demons fire their signature ability simultaneously. Dark Command cuts CD, adds 10% haste 6s.
- **Legion Standard** — a **creature**, not a gameobject: destructible, ~40% owner max HP, CC-immune. Pulses demon buffs in 20yd (30 with Fervent Standard), movement speed to party. Model on totem behaviour.
- **Soul Link** — 20% of player damage taken split across demons; +5% demon damage per demon beyond the first. Routes through `OnPoolChanged`.
- **Metamorphosis (Beacon)** — **new spell ID, do not reuse 47241.** Roots and disarms the caster, +50% demon damage, −50% demon damage taken, no GCD on Demonic Empowerment for the duration.
- **Fel Lash** — ~15% SP damage plus a 6s +20% demon-damage debuff, **caster-scoped**.

---

## 7. Phase 5 — The talent tree

### 7.1 Manifest first, MCP second

`data/spellforge/talents/` describes all 36 nodes: tier, column, ranks, rank spell IDs, prerequisites, points available. `build.sh` drives SpellForge (`sf build`) from it. `tools/validate_tree.py` runs first and **blocks the build on failure** — SpellForge resolves client-side concerns but will not know Tier 3 is supposed to have exactly 4 nodes and 8 available points.

Validator assertions:
- Node counts per tier match `4-3-4-3-4-3-4-4-3-3-1`.
- Points per tier match `10-8-8-7-8-8-8-9-6-5-1`, summing to **78**.
- Every design §5 prerequisite present, each prereq at a tier ≤ its dependent.
- The §6 reference build (51 points) is legally spendable — walk it point by point, asserting each gate at the moment of spend.
- Every rank references a spell defined under `data/spellforge/spells/`.

### 7.2 Layout details that bite

- **Row index drives the gate.** The client computes points-required as `Row * 5`, matching the design's 0/5/10/…/50 exactly. No special handling.
- **Columns must make arrows drawable.** The 3.3.5 frame renders clean arrows for vertical, horizontal and simple L-shapes only; a prereq two rows up and two columns across renders as garbage. Give the Command chain (Fel Conditioning → Expanded Command → EC II → Eternal Servitude → Legion Commander → capstone) **one column top to bottom** — it's the spine — with the Empowerment chain (Unholy Vigor → Cruelty of the Pit → Ruinous → Supreme) adjacent.
- **Improved Legion's ranks differ in kind** (rank 1 cast time, rank 2 shard cost). Legal, but auto-generated tooltips read strangely. Hand-write both, check in-game.
- Target the Warlock Demonology tab — read the real ID from `TalentTab.dbc`, don't assume. Replacing in place; `ChrClasses` fixes the tab count at 3.

### 7.3 Talents needing C++

Soul Harvest, Cruel Master, Expanded Command I/II, Legion Commander, Improved Legion, Rapid Conjuration, Demonic Rebirth, Wrath of the Legion, Bound by Blood, Eternal Servitude, Supreme Empowerment, Grand Warlock's Design, Vicious Pact, Fel Blood's Corruption refresh, Blood Tithe.

The remaining ~18 are plain aura effects. **Prefer reading talent rank at point of use over syncing auras across four moving pets** — far fewer state bugs.

---

## 8. Phase 6 — Stat inheritance

Where the gearing story lives (design §9.4).

Core patch: add a hook/virtual at `Guardian::UpdateAttackPowerAndDamage()` and `Guardian::UpdateStats()` in `StatSystem.cpp`. New creature entries inherit **nothing** by default — skip this and the pack is flat, gear-independent and worthless at 80.

`PetScaling::ApplyInheritance(Player*, Creature*)`:
1. Read owner SP, haste, crit, hit.
2. Apply design coefficients — Wild Imp Firebolt ~28% SP/bolt, rift demons ~22% SP, Doomguard Doom Bolt ~64% SP, Felhunter ~35% SP→AP.
3. Layer Vicious Pact (`8/16/24%` SP→AP, `5/10/15%` SP→SP), Fel Armory, Savage Instincts.
4. Inherit haste/crit/hit at 100%.

Re-run on: summon, equipment change, owner aura change affecting SP (trinket procs), talent change.

Also required: `pet_levelstats` rows for every new creature entry, or base health at 80 is nonsense.

---

## 9. Phase 7 — Capstone

**Grand Warlock's Design** (T11), all four effects routed through `OnPoolChanged`:
1. **Legion Aura** — 5% party/raid damage and haste while controlling 3+ demons. Apply/remove on pool change *and* group change, with a slow reconciliation timer.
2. Demonic Empowerment loses the GCD — conditional `SpellInfo` override on cast.
3. First summon after entering combat is free and instant — combat-entry flag, cleared on leaving combat.
4. +4% demon damage per demon.

---

## 10. Phase 8 — Config and QA

### 10.1 Migration

Solo realm, one character, backup in hand. No announcements, no refunds, no rehearsal:
- Delete Demonology-tab `character_talent` rows.
- Set the `RESET_TALENTS` at_login flag (value from `AtLoginFlags` in `Player.h`).
- Clear saved pets that are now invalid.

If a talent apply goes wrong, restore from backup (or SpellForge's `build/backups/<ts>/`) and re-run `build.sh` + `install.sh` — which is the practical reason the YAML source and vendored `dist/` exist.

### 10.2 Config

Every number the design states should be tunable, since none survive playtesting:
`Enable`, `SoulHarvest.ChancePerRank`, `SoulHarvest.InternalCooldownMs`, `CruelMaster.CritMultiplier`, `WildImp.Count/Duration/SPCoefficient`, `DemonicEmpowerment.Haste/Damage/Duration`, `LegionStandard.HealthPctOfOwner`, `Threat.NonAnchorMultiplier`, `LegionAura.DamagePct/HastePct`, `Beacon.DemonDamagePct`, `Debug.LogShardIncome`.

### 10.3 Test matrix — what one player can actually run

| Scenario | Catches |
|---|---|
| `.levelup` through 10 → 20 → 30 → 40 | Design §11 leveling beats land at the right levels; low-rank shard starvation |
| Full 4-demon pack, logout/login | Pool persistence |
| Respec 4 slots → 1 | Orphaned demons, stuck auras, Felguard leaking to a Destro build |
| Level 80 dummy parse, 5 min | Shard income rate + first balance datapoint |
| Duel/dummy as Affliction and Destruction | No baseline spell was nerfed — the design's core constraint |
| GM-spawned trash pack | Wild Imp churn, Bound by Blood refund loop, no infinite resummon |
| Solo dungeon run | Anchor mirroring under real pathing; Command Demon Felstorm behaviour |
| `.legion dumpstats` after gear swaps | Inheritance re-runs on equipment change |
| Disconnect mid-pack | No orphaned creatures left in world |

**Known untestable on this setup:** raid-scale performance (20 warlocks, 80+ AI-ticking creatures with per-hit proc rolls) and Legion Aura across a real raid group. Batch pool updates on a 250–500ms cadence from the start rather than every tick, so the cost is bounded by construction rather than by later profiling.

---

## 11. Risk register

| Risk | Severity | Mitigation |
|---|---|---|
| Fork drift — upstream merge silently drops a hook | **High** | `core-patches/CORE_PATCHES.md` + smoke test per patch; rebase, don't merge |
| ~~MCP not idempotent~~ | **Resolved** | SpellForge is deterministic + manifest-hashed; `sf diff_build` dry-run; auto-backup on deploy |
| ~~ID collision with existing customs~~ | **Resolved** | Audited empty; IDs assigned in SpellForge ranges (§1 Task 1); `check_ids.py` enforces |
| Vendored `dist/` drifts from `data/spellforge/` YAML | Medium | `build.sh` always regenerates `dist/`; CI/`check_ids.py` can diff manifest hash |
| Pet AI cost at scale | Medium (deferred) | Batch updates by construction; untestable solo |
| Soul Harvest mistuned | Medium | Config-driven, logged, measured against the doc's rate |
| Stuck Legion Aura | Medium | Single `OnPoolChanged` path + reconciliation timer |
| Mirroring AI feels unresponsive in play | Medium | Deferred per-demon control path documented in §3.1 |
| Multi-effect rank tooltips (Improved Legion) | Low | Hand-written tooltips |

---

## 12. Sequencing

1. **Phase 0** — ~~ID audit, MCP probe~~ (both resolved), skeletons + `build.sh`/`install.sh`/`dist` plumbing, vertical slice. *Gate: one talent learnable in-game, one summon working.*
2. **Phase 1 — ✅ COMPLETE (verified in-game 2026-08-16).** Command Pool + mirroring DemonAI (attack the anchor's target, follow/stay with it, formation spacing, Seduce CC-awareness, no-evade neutral engage, assist/follow lifecycle), `.legion recruit/dismiss/pool`, and persistence (`character_legion_slots` save/restore incl. health%). *Gate met: demons mirroring the anchor, persistent across logout/login, clean on disconnect.* Deferred polish: threat auras / Legion Aura via `OnPoolChanged` (Phase 7 territory); `SetMinion` core-patch not needed (guardians coexist with the pet).
3. **Phase 2** — Shard economy. *Gate: measured income on target.*
4. **Phase 3–4** — Summons, then empowerment kit.
5. **Phase 5** — Full tree via manifest + validator.
6. **Phase 6** — Inheritance. *Gate: level 80 dummy parse in a sane range.*
7. **Phase 7** — Capstone.
8. **Phase 8** — Config sweep, test matrix.

Phases 0–2 are the ones that can still invalidate the design. Nothing broad starts until they're green.
