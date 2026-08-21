# PLAN — Playerbots Integration for mod-demonology-rework

**Goal:** Demonology warlock bots (mod-playerbots @ `085e127e`, restructured `src/Ai/` layout) pick sensible talents in the custom 78-point tree, know all rework abilities, and execute a competent rotation built around the pool → Doombrand → Command Demon loop.

**Audience:** implementation by Claude Code. **Analysis before code is mandatory** — Phase 0 produces a written rotation & talent spec that the user reviews before any strategy code is written.

---

## 0. Ground truth (verified, do not re-derive — but DO re-verify file paths on the live checkout)

### 0.1 What the rework gives bots to work with

| Mechanic | Bot-relevant facts |
|---|---|
| Anchor pet + Command Pool | Base 0 legionnaire slots; ec/ec2/lc talents each +1 (max 3, lc makes 4th). Legionnaires mirror the anchor's target/react state. |
| Command Demon `290013` | Baseline. 1 shard, 45s CD (dc: −5/10/15s), instant, on GCD. Requires living, actionable anchor; needs a hostile target for felguard/felhound/succubus/imp anchors (VW press works targetless). Signature from anchor + free echoes from every legionnaire. |
| Doombrand `290014` | Talent-granted (gwd capstone). 1 shard, 20s CD, 10s debuff `290504`. Stores 15% (conf) of **demon** damage to the branded target, detonates on expiry/death. Skill = packing the army's best 10s inside the window. |
| Summon Wild Imps `290001` | Trainer @10. 2 shards (il reduces to 1). Path B core active. |
| Demonic Empowerment `290000` | Trainer @50, rank-less. Buffs pet + legionnaires + wild imps + greater demon. uv/se extend, sl shields, cotp/re amplify. |
| Legionnaire summons `290002–290006` | Talent/known-pet hybrid-learned (imp/felguard/succubus/felhound/voidwalker). Resummon costs 1 shard. Felguard (pet and legionnaire) gated by sfg (T7). |
| Greater demons `290007/290008` | Infernal 2 slots, Doomguard 3 slots; es (T10) makes permanent. |
| Shard economy | Path B: shards are ordinary bag items, **uncapped by design, hoarding is fine**. Generation via Soul Harvest talent (demon damage, ICD ~1/8–12s with 2–3 demons); cm accelerates on crits; rc makes the warlock's own Corruption count as demon damage (½ weight); bbb refunds on demon death. |
| Cut abilities | Metamorphosis, Immolation Aura (as demo identity), Hand of Gul'dan, demon charge. Demo is a **ranged commander**, never melee. |
| Tree | Tab 303, 36 nodes, 11 tiers, layout `[4,3,4,3,4,3,4,4,2,4,1]`, 78 total points. Prereqs: cm←sh, rc←sh, bt←vc@2, vp←si@3, bor←es, es←lc, se←re@3. Level 80 = 71 points → the full tree is unaffordable; build choice is real. |
| Tunables | All in the live `.conf`; `docs/balance_model.py` is the EV model (FLATTEN pass applied). |

### 0.2 What playerbots @ 085e127e actually does (verified in source)

- **Layout:** warlock AI lives in `src/Ai/Class/Warlock/` — `WarlockActions.{h,cpp}`, `WarlockTriggers.{h,cpp}`, `WarlockAiObjectContext.{h,cpp}`, `Strategy/{Generic,Affliction,Demonology,Destruction,Tank}WarlockStrategy.*`, `Strategy/GenericWarlockNonCombatStrategy.*`. Factories: `src/Bot/Factory/{PlayerbotFactory,AiFactory,RandomPlayerbotFactory}.cpp`. Config: `src/PlayerbotAIConfig.{h,cpp}`.
- **Spec→strategy:** `AiFactory.cpp` maps most-points tab → `specName = "demo"` and adds combat strategies `"demo"`, `"curse of agony"`, **`"meta melee"`** (line ~383) plus non-combat `"felguard"`, `"spellstone"` (~line 565). `RandomClassSpecProb/Index` conf keys drive random-bot spec choice.
- **Talents:** `AiPlayerbot.PremadeSpecLink.<classId>.<specIdx>.<level>` strings are parsed by `PlayerbotAIConfig::ParseTempTalentsOrder` (~line 924) against the **live `sTalentStore`/`sTalentTabStore`**, talents bucketed per `tabpage` and (implicitly) ordered by tree position. `PlayerbotFactory::InitTalentsBySpecNo` / `InitTalentsByParsedSpecLink` (~lines 1558/1644) walk level breakpoints downward until a non-empty parsed link is found and apply points in order. **Consequence:** the custom tab is reachable through existing machinery; every warlock demonology link string (all level breakpoints, plus the RANDOMBOT DEFAULT TALENT SPECS section of `playerbots.conf.dist`) is invalid for the new tree and must be regenerated positionally.
- **Actions cast by name:** strategy actions are string-keyed (`"shadow bolt"` etc.) and resolve against the bot's known spells by name. Custom SpellForge spells are castable this way **iff** the bot knows them and the DBC spell name matches the action string.
- **Spell learning:** `PlayerbotFactory::InitializeBot` calls `InitAvailableSpells()` (trainer-cache driven) before and after `InitTalentsTree()`. Trainer-taught rework spells (Wild Imps @10, Demonic Empowerment @50 on warlock trainers 31/32) should flow through this; hybrid-learned summons depend on the rework's talent-learn hook firing for factory-applied talents. Both need runtime verification.
- **Hazards found:**
  1. `TooManySoulShardsTrigger::IsActive()` returns `count >= 26` and drives `"destroy soul shard"` in **both** `GenericWarlockStrategy` (~line 76) and `GenericWarlockNonCombatStrategy` (~line 85). Under Path B this destroys the fuel supply.
  2. `"meta melee"` sends demo bots into melee; `DemonologyWarlockStrategy` prioritizes `metamorphosis`, `immolation aura`, `demon charge`, `soul fire`, `immolate/incinerate` — a stock-WotLK rotation that is wrong end-to-end for the rework. (Bots won't know the cut spells so most triggers dead-fire, but `meta melee` positioning and the immolate/incinerate default actions actively degrade play.)
  3. `WrongPetTrigger` + `pets[]` table + default `"felguard"` pet strategy assume felguard availability; under the rework felguard needs sfg (T7), so pre-T7 bots need a different default anchor.
  4. `OutOfSoulShardsTrigger` at 0 shards presumably drives Drain Soul farming — mostly redundant once Soul Harvest is talented, but the correct floor is now "reserve for actives," not "have ≥1."
  5. Factory re-init / respec of thousands of random bots interacts with the known **orphaned `character_talent` crash-loop** gotcha. Any talent-string mistake deployed fleet-wide is a server-killer; the spec-link generator must be validated against the shipped `Talent.dbc` before any bot logs in with it.

### 0.3 Where the code lives & how it deploys (REQUIRED — follow the module's script-driven conventions)

All playerbots changes are **authored, vendored, and installed from inside `mod-demonology-rework`**, exactly like everything else in the module: nothing is hand-copied into the playerbots checkout, and an archive of the rework repo installs offline. Concretely:

```
playerbots-patches/            versioned NNN-description.patch files, generated against the
                               PINNED commit 085e127e (record the hash in a MANIFEST here)
install_playerbots.sh          dedicated installer (dry-run by default, --apply to execute),
                               sibling to install.sh / deploy_client.sh
conf/playerbots-demonology.conf.fragment
                               vendored spec-link strings + any new bot tunables, emitted by
                               tools/gen_playerbot_spec_links.py (generated → vendored, same
                               model as the SpellForge dist/ artifacts)
tools/gen_playerbot_spec_links.py   the generator (Phase 1); its output is checked in
```

**Patch-based, not file-replacement.** A working fork branch of mod-playerbots is fine for development, but the shipping artifact is the set of versioned patches: `git diff 085e127e` split into logical patches mirroring the existing `core-patches/` style (numbered, one concern each, apply/revert-safe). File replacement silently clobbers upstream state and can't detect drift; `git apply --check` against a pinned commit can.

**`install_playerbots.sh` contract** (mirror `install.sh` behavior):
- Dry-run by default; `--apply` executes; `--revert` backs the patches out.
- Verifies the target mod-playerbots checkout is at the pinned commit (or that all patches pass `git apply --check` / `--reverse --check`) before touching anything; refuses with a clear message on drift.
- Backs up before modifying, applies patches idempotently (re-running is a no-op).
- Merges `conf/playerbots-demonology.conf.fragment` into the live `playerbots.conf` inside a managed marker block (`# BEGIN/END mod-demonology-rework`) so re-installs replace the block instead of appending duplicates.
- Does **not** compile — it prints the reminder that patched C++ requires the standard step-2 rebuild (`cmake` only if new .cpp files were added; `make -j$(nproc) && make install`) and a worldserver restart, matching the README's workflow numbering.
- Takes the playerbots checkout path as an argument or env var with a sensible default for this server; never hardcodes it in more than one place.

Update the rework README's Build & Deploy workflow with the new step so the process stays discoverable: author (generator → vendored fragment) → compile (if C++ touched) → `./install_playerbots.sh --apply` → restart.

Runtime-gate all rework behavior on capability, not build flags — e.g. a helper `IsDemonologyRework(bot)` ≈ `sSpellMgr->GetSpellInfo(290013) && bot->HasSpell(...)` checks — so the patched playerbots still behaves stock on a vanilla DBC. Conf entries go in `playerbots.conf` via the managed block (spec links, bot tunables); anything shared with the module's own systems goes in the module `.conf` per "tuning lives in conf".

---

## Phase 0 — Analysis & written spec (NO CODE)

Read, in order: `DEMONOLOGY_DESIGN_V2.md`, `DEMONOLOGY_STATUS.md`, `docs/balance_model.py` (+ run it), the live module `.conf` (actual tunables: Command CD, StorePct, cap coef, Soul Harvest ICD, echo values), `data/spellforge/ids.yaml`, `demonology_tree_spec.md`, and the shipped `Talent.dbc`/tree SQL in `dist/` (authoritative node positions after the reshuffle). Then read the playerbots files listed in §0.2 end to end.

Produce `docs/BOT_INTEGRATION_ANALYSIS.md` in the rework repo containing:

1. **Talent builds.** A level-80 71-point ST build with per-node justification grounded in `balance_model.py` numbers (extend the model if needed rather than hand-waving). Expected shape to validate, not assume: slots (ec/ec2/lc) and the gwd capstone chain are mandatory; flat demon multipliers (fa, pf, vp+si, cotp) and economy (sh, cm, rc) next; likely cuts among wl, fb, gb, iwi/wotl, dr/bbb, il, and part of the Empowerment spine (uv/re/se) — arbitrate with the model, including the Doombrand-window interaction (spine multipliers convert into stored burst). Justify every cut node with a number or a mechanic. Then derive **leveling breakpoints** (at minimum 10/20/30/40/50/60/70/80) respecting tier gates (5 pts/tier) and all prereq edges (§0.1), ordered for leveling quality: early economy (sh) + first slot (ec, T3) + cheap multipliers before deep tiers. Note where the anchor changes (VW/imp → felguard at sfg).
2. **Rotation spec.** Priority list with exact conditions, e.g. (to validate against real conf values, not to copy blindly):
   - Precombat/non-combat: Fel Armor up (fa synergy) · anchor pet out (felguard if sfg else voidwalker pre-50 / imp) · legionnaire slots filled to cap, resummon on death if shards > reserve · Soulstone/healthstone per stock behavior.
   - Combat: maintain Corruption (mandatory once rc is talented — it feeds shards and half-weight brand charge) · Doombrand on CD when shards ≥ 1 + reserve logic · Command Demon **inside an active brand window when the CDs allow it** (with dc@3, 30s Command vs 20s brand aligns every other brand; quantify the value of holding Command up to N seconds for a brand vs pressing on CD — pick N from the model) · Demonic Empowerment inside the brand window when possible, else on CD · Summon Wild Imps when shards ≥ cost + reserve (reserve = 2: one brand + one command) · Life Tap when mana < X% **or** (vc talented and pool average HP < Y%) with a health floor · Shadow Bolt filler · shoot fallback. AoE: weak by design — Seed of Corruption/Rain of Fire only if actually known; do not force an AoE mode.
   - Low-level ladder: what the rotation is at 10–29 (pet + Corruption + Wild Imps + Shadow Bolt), 30–49, 50–69 (add DE), 70+ (Command), 75/80 (gwd → full loop). Bots exist at every level; the strategy must degrade gracefully.
3. **Shard policy.** Reserve floor, Wild Imp gating, whether 26-destroy should become never / a much higher bag-pressure ceiling for rework bots, and what replaces Drain-Soul-farming.
4. **Anchor policy.** Which anchor per role (ST damage = felguard; a note on TankWarlockStrategy/VW anchor as explicitly out of scope unless the user opts in).
5. **Open questions for the user** (see §Open decisions below) — stop and ask before Phase 3 if any materially change the design.

**Accept:** user has reviewed and signed off on the analysis doc.

## Phase 1 — Talent plumbing

1. Write `tools/gen_playerbot_spec_links.py` in the rework repo: reads the shipped tree (dist Talent.dbc or its SQL/YAML source of truth), takes a build definition (ordered node-key → ranks per level breakpoint from Phase 0), validates prereqs + tier gates + point totals, and **emits** the `AiPlayerbot.PremadeSpecLink.9.<demoSpecIdx>.<level>` strings positionally against the same row/col ordering `ParseTempTalentsOrder` uses. Refuse to emit on any validation failure. Never hand-write link strings. Output goes to `conf/playerbots-demonology.conf.fragment` (vendored, checked in) — the installer owns getting it into the live conf, per §0.3.
2. Confirm the demonology spec index and `RandomClassSpecProb/Index` mapping for warlocks at this commit; update the conf so random warlock bots roll demonology at whatever probability the user wants (ask; default: keep stock distribution).
3. Also regenerate/neutralize the RANDOMBOT DEFAULT TALENT SPECS warlock demonology entries in `playerbots.conf.dist` if that path is exercised at this commit (verify which path random bots actually take).
4. Safety: before fleet deploy, dry-run on ONE bot; verify `character_talent` rows written all exist in the shipped Talent.dbc (SQL check), then restart-cycle once to prove no crash-loop (the orphaned-talent gotcha).

**Accept:** a fresh level-80 demo bot initializes with exactly the Phase-0 build (verify via in-game inspect + `character_talent` query); a level-30 bot gets the 30 breakpoint; server survives a restart with bots spec'd.

## Phase 2 — Spell knowledge

1. Verify factory-initialized bots know: Wild Imps (any bot ≥10), Demonic Empowerment (≥50), Command Demon (baseline — confirm how baseline `290013` is granted and that bots get it), hybrid-learned legionnaire summons + Doombrand matching their talents. Chase order-of-operations: `InitAvailableSpells` → `InitTalentsTree` → does the rework's hybrid-learn hook fire on the factory's talent-application path? Fix on whichever side is cleaner (prefer a rework-side hook that reconciles learned spells from talents on login/levelup, which also heals live bots).
2. Verify name-based action resolution finds each custom spell (exact DBC names; watch for duplicate names against stock spells — e.g. any rework spell named identically to a stock one will need ID-pinned actions instead of name lookup).

**Accept:** `.playerbots bot add` a demo bot, dump its spellbook; all rotation spells present at appropriate levels; a hand-issued cast command for each custom spell succeeds.

## Phase 3 — Non-combat strategy

New actions/triggers in `src/Ai/Class/Warlock/` (registered in `WarlockAiObjectContext`), gated on rework presence:

- `summon legionnaire <type>` actions + a `legion pool not full` trigger (query the rework's Command Pool for the bot — expose a small read API from the rework module if none exists; do not duplicate pool math in playerbots).
- Anchor selection replacing the stock felguard default: felguard if known, else voidwalker (leveling survivability) — implemented via the existing pet-strategy mechanism so users can still override with pet strategies.
- Shard policy: neuter/raise `TooManySoulShardsTrigger` for rework bots per Phase 0; remove Drain-Soul-farm behavior for rework demo bots.
- Fel Armor preference in the armor chain (stock `DemonArmorTrigger` already accepts fel armor; ensure the cast preference is fel armor when known, for fa).

**Accept:** out of combat, a bot fills its pool to talent cap within ~30s, resummons a killed legionnaire, keeps Fel Armor, and its shard count grows across a grind session (never destroyed below the policy ceiling).

## Phase 4 — Combat strategy

Rework `DemonologyWarlockStrategy` (runtime-gated; stock path preserved verbatim for vanilla):

- New triggers: `doombrand ready` (CD + shard + target-will-live heuristic), `doombrand active on target` (aura `290504` present from this bot), `command demon window` (brand active OR hold-timer exceeded per Phase-0 N), `demonic empowerment window`, `wild imps affordable`, `legion needs heal` (vc life-tap condition via pool HP), plus Corruption maintenance already exists.
- Priorities per the Phase-0 spec; default actions end in `shadow bolt` → `shoot`.
- Remove for rework bots: `meta melee` (AiFactory gating), metamorphosis/immolation aura/demon charge/soul fire/immolate/incinerate trigger wiring and default actions.
- Ranged positioning: ensure the demo bot uses caster range/LoS behavior (whatever `GenericWarlockStrategy` provides once `meta melee` is gone).

**Accept:** on a target dummy at 80: Corruption uptime >95%; a brand every ~20–25s; ≥80% of Command presses land inside a brand window when CDs align; Empowerment on CD or in-window; shards never hit 0 mid-fight in sustained combat ("wanted to press but couldn't afford" ≈ never — the design's own feel metric); zero melee approaches.

## Phase 5 — Validation & tuning

1. Solo grind test (level ~30, ~60): bot kills mobs continuously for 15 min without dying, pool stays full, no shard starvation.
2. Dungeon/party smoke test at 80 with the user driving: `.legion dumpstats` after pulls to inspect press breakdowns/brand charge; compare bot DPS vs the module's modeled +30%/+45% target using whatever parse method the user already uses for balance validation.
3. Fleet test: spawn 20+ random demo bots; watch worldserver perf (each bot = up to 5 demons + wild imps — if creature counts hurt, add a conf knob capping bot legionnaire slots, default uncapped) and error log for name-resolution or dead-trigger spam.
4. Installer round-trip test: from a pristine mod-playerbots checkout at `085e127e`, `./install_playerbots.sh` (dry-run) reports cleanly → `--apply` patches and merges the conf block → rebuild → server boots with working demo bots → `--revert` restores pristine (verify with `git status`/`git diff`) → `--apply` again succeeds (idempotence). Also verify a second `--apply` on an already-patched tree is a clean no-op and that a deliberately drifted checkout is refused with a useful message.
5. Deliver: updated `docs/BOT_INTEGRATION_ANALYSIS.md` (as-built), the versioned patches in `playerbots-patches/` with pinned-commit MANIFEST, `install_playerbots.sh`, the vendored conf fragment, the spec-link generator, and the README workflow update per §0.3.

---

## Open decisions (ask the user during Phase 0)

1. Random-bot demonology probability — leave stock or bias toward the new spec?
2. Should bots ever use greater demons / the es-permanent Infernal-Doomguard endgame line, or is the bot build strictly the legionnaire line? (Affects the 71-point build.)
3. Warlock-tank (VW anchor + Suffering) bot support — in scope?
4. PvP/battleground behavior — out of scope unless stated?
5. Bot legionnaire cap conf knob default, if perf demands one.

## Standing constraints

- No hardcoded balance numbers — thresholds that are tuning-flavored go to conf.
- Never edit stock behavior for non-rework installs; every change runtime-gated.
- Respect the repo's patch/apply-revert + vendored-artifact conventions.
- The orphaned-`character_talent` gotcha applies to any talent-string error at fleet scale: validate before boot, always.
