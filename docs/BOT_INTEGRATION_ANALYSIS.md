# Playerbots Demonology Integration — Phase 0 Analysis

**Module:** mod-demonology-rework (COMPLETE & LIVE) · **Target:** mod-playerbots @ `085e127e`
**As of:** 2026-08-20 · **Status:** Phase 0 (analysis only — no code). Awaiting user sign-off.

Ground truth re-verified against the live checkouts (paths/lines below are confirmed, not assumed).
Numbers are driven by `docs/balance_model.py` (live-synced) + `docs/build_marginals.py` (added this phase).

---

## 0. Verified ground truth (re-checked on the live trees)

**Rework tunables (live `.conf`, not doc defaults):**
- Command Demon `290013`: 45s CD, 2 shards + 5% base mana, instant/GCD. Dark Command (dc) −5s/rank → **dc@3 = 30s CD**.
- Doombrand `290014` (gwd capstone): **20s CD**, 1 shard, 10s debuff `290504`, stores **0.35 ×** demon
  damage to the branded target, cap = 5.0 × SP, detonates on expiry/death.
- Summon Wild Imps `290001`: **trainer @10**, 1 shard, 30% base mana (il −15/−30%).
- Demonic Empowerment `290000`: **trainer @50**, rank-less, 12s base (+uv 1/2/3s, +se 3/6s → up to 21s), 60s CD.
- Legionnaire summons `290002–290006`: hybrid-learned on talent/known-pet; 1 shard each.
- Soul Harvest: 4%/rank on demon damage, 1s ICD. cm accelerates; rc makes Corruption count (½ weight).
- Pool: base 0 legionnaires; ec/ec2/lc each +1 → **3 legionnaires + anchor** at full spine.

**Playerbots @085e127e (verified in source):**
- Warlock AI: `src/Ai/Class/Warlock/` (+ `Strategy/`), factories `src/Bot/Factory/{AiFactory,PlayerbotFactory,RandomPlayerbotFactory}.cpp`, config `src/PlayerbotAIConfig.cpp`.
- `AiFactory.cpp:272` demo specName; **`:383` adds `"meta melee"`** (hazard); `:565` non-combat `"felguard"`.
- `DemonologyWarlockStrategy.cpp:15-27` = stock metamorphosis/immolate/incinerate/soul fire rotation (hazard, wrong end-to-end).
- `WarlockTriggers.cpp:49` **`TooManySoulShardsTrigger ≥ 26 → destroy`** (hazard vs Path B); `:47` OutOfSoulShards == 0; shard item 6265.
- `PlayerbotAIConfig.cpp:924` `ParseTempTalentsOrder`; `:482` `PremadeSpecLink.<cls>.<spec>.<level>` format; `:501/512` RandomClassSpecProb/Index. (Demo spec index for cls 9 to be pinned in Phase 1.)

---

## 1. Talent build — level 80, 71 points (single-target)

**Per-node single-target value at level 80** (from `docs/build_marginals.py` — Δ DPS when the node is removed):

| Rank | Node | Δ DPS | % of ST | Verdict |
|---|---|---:|---:|---|
| — | Imp-legionnaire throughput (the 3 imp legionnaires) | 996 | 22% | slots mandatory |
| — | Wild Imp throughput (Summon Wild Imps + iwi/wotl) | 776 | 17% | keep engine |
| gwd | Doombrand | 547 | 12% | capstone, keep |
| fa | Fel Armory | 479 | 11% | keep |
| vp | Vicious Pact | 479 | 11% | keep |
| uv/se | Empowerment spine uptime | 450 | 10% | keep spine |
| cotp | Cruelty of the Pit | 303 | 7% | keep |
| fs | Fervent Standard | 272 | 6% | keep |
| si | Savage Instincts | 208 | 5% | keep |
| gb | Grim Bargain | 128 | 3% | keep |
| pf | Pactbound Fury | 92 | 2% | keep |
| **bor** | **Beacon of Ruin** | **0** | **0%** | **CUT — no greater demon in a legionnaire build** |

Not in the damage model (argued by mechanic): **ec/ec2/lc** = a whole legionnaire each (mandatory —
they ARE the 22% "imp-legionnaire" line); **sh/cm/rc** = the shard engine that gates the entire
active loop (mandatory); **vc/bt** = self-heal; **wl** = CC immunity; **dr/bbb** = death recovery +
shard refund; **il** = Wild Imp mana; **iwi/wotl** = feed the Wild Imp line.

### The 71-point build (7 cuts, all justified)

**CUTS (7 pts):** `bor` (2) — **0% ST** (no greater demon); `es` (1) — enables the unused greater-demon
line (and is bor's prereq); `cv` (2) — pure stamina, **0% ST**; `fb` (2) — Succubus Lash bonus, **~0%**
in a felguard+imp comp (no Succubus). Everything else is taken at max rank.

```
T1  Fel Armory 3 · Fel Conditioning 3 · Improved Legion 2                    (8)
T2  Pactbound Fury 3 · Vital Conduit 2                                       (5)   [cv, fb cut]
T3  Soul Harvest 3 · Cruel Master 2 · Fel Corruption 3 · Expanded Command 1  (9)
T4  Savage Instincts 3 · Improved Wild Imps 2 · Shadowflame Legion 2         (7)
T5  Vicious Pact 3 · Blood Tithe 2 · Warded Legion 2 · Expanded Command II 1 (8)
T6  Dark Command 3 · Unholy Vigor 3 · Demonic Rebirth 2                      (8)
T7  Wrath of the Legion 3 · Grim Bargain 2 · Fervent Standard 2 · Summon Felguard 1 (8)
T8  Cruelty of the Pit 3 · Overlord's Presence 3 · Fel Conduit 2 · Riftwalker 1 (9)
T9  Ruinous Empowerment 3 · Bound by Blood 2                                 (5)
T10 Supreme Empowerment 2 · Legion Commander 1                              (3)   [es, bor cut]
T11 Grand Warlock's Design 1                                                (1)
                                                                        TOTAL 71
```
Prereqs satisfied: cm/rc←sh (all T3), bt←vc@2, vp←si@3, se←re@3, ec2←ec, gwd←lc. T1–T10 = 70 pts ≥ the
gwd (T11) 50-point gate. **Why keep dc@3:** it takes Command Demon to 30s CD, which is what makes the
brand-window alignment (below) achievable. **Why keep dr/bbb/il/wl over more raw DPS:** a bot lets
demons die and doesn't play perfectly — resummon-on-death, the death shard-refund, mana efficiency,
and demon CC-immunity all protect *uptime*, which is worth more to an imperfect bot than the ~2–3% ST
of the marginal damage points they'd replace.

---

## 2. Leveling breakpoints (10 → 80)

Talent points = level − 9 (1 at L10 → 71 at L80). Tier gate: a point in tier N needs 5·(N−1) spent.
Order = economy + first slot + cheap multipliers first; deep tiers last. Every point is a subset of
the final build (no wasted/respec points).

| Level | Pts | Key state (cumulative) | Milestone |
|---|---:|---|---|
| 10 | 1 | fa 1 | Wild Imps trained; anchor = **Voidwalker** (survivable) |
| 15 | 6 | fa 3, fc 3 | demon dmg + HP online (T2 open) |
| 20 | 11 | +Expanded Command 1, Soul Harvest 3 (start) | **first legionnaire slot + shard engine** (T3 open) |
| 30 | 21 | +Cruel Master 2, Fel Corruption 3, Savage Instincts 3, Improved Wild Imps 2 | economy complete; **maintain Corruption** now |
| 40 | 31 | +Vicious Pact 3, Expanded Command II 1, Dark Command…, **Summon Felguard 1** | 2nd slot; **anchor → Felguard**; T7 reached |
| 50 | 41 | +Unholy Vigor, Wrath of the Legion, Grim Bargain, Fervent Standard | **Demonic Empowerment trained @50**; spine online |
| 60 | 51 | +Cruelty of the Pit 3, Overlord's Presence 3, Fel Conduit 2, Riftwalker 1 | T11 gate met |
| 70 | 61 | +Ruinous Empowerment 3, Bound by Blood 2, Supreme Empowerment 2, **Legion Commander 1** | **3rd slot** (full army) |
| 75 | 66 | +Grand Warlock's Design 1 (as soon as T11 affordable) + filling | **Doombrand → full loop** |
| 80 | 71 | complete 71-pt build | — |

(These are the plan's minimum checkpoints; the Phase-1 generator emits a string per breakpoint so any
level in between gets the highest-affordable build.)

---

## 3. Rotation spec (priority, top to bottom)

**Reserve = 2 shards** (protect one Doombrand + one Command). Costs: Doombrand 1, Command 1, Wild Imps 1.

**Out of combat / pre-pull:**
1. Fel Armor up (feeds fa; also Demon Skin/Demon Armor count).
2. Anchor pet out (Felguard if sfg known, else Voidwalker).
3. Fill legionnaire slots to cap with **Imp legionnaires**; resummon a dead one if shards > reserve.
4. Soulstone/healthstone per stock behavior.

**Combat (priority):**
1. **Corruption** on target if missing (mandatory once rc talented — feeds shards + ½-weight brand).
2. **Doombrand** when ready (20s CD) and shards ≥ 1 + reserve-aware, and the target will live ≳ the window.
3. **Command Demon** — press if a brand is currently active; else if the next brand is ≤ `CommandBrandAlignWindowMs`
   (conf, default **4000 ms**) away, hold; else press on CD. (30s Command vs 20s brand with dc@3 aligns
   often; the align window buys the stored-detonation value without throwing away Command uptime.)
4. **Demonic Empowerment** when ready — prefer inside a brand window, else on CD.
5. **Summon Wild Imps** when shards ≥ 1 + reserve (i.e. shards > 2) — overflow spender.
6. **Life Tap** when mana < 20% **or** (vc talented and pool avg HP < 50%), gated by an own-HP floor (≥ 30%).
7. **Shadow Bolt** filler.
8. **Shoot** (wand) fallback when nothing else is castable.

**AoE:** weak by design (−30/−55% vs Affliction in the model). Use Seed of Corruption / Rain of Fire
**only if actually known** (base warlock spells); do not force an AoE mode. The Felguard anchor's cleave
+ Corruption spread is the incidental AoE.

---

## 4. Shard policy

- **Neuter `TooManySoulShardsTrigger` for rework bots** — Path B is uncapped by design, hoarding is
  intended. Recommend: rework bots never auto-destroy shards (or a very high ceiling ≥ the soul-bag
  capacity). Conf-gated (`AiPlayerbot...` in the managed block); stock bots keep the 26 behavior.
- **Reserve floor = 2** (one brand + one command). Wild Imps only spend above it.
- **Replace Drain-Soul farming:** once Soul Harvest is talented (~L20) generation is passive; keep a
  Drain-Soul-on-killing-blow fallback **only** when shards == 0 and a brand/command is wanted (mostly
  pre-L20 or after a long drought). "Have ≥1" is the wrong floor; "reserve for actives" is the floor.

---

## 5. Anchor policy

- **ST default:** Felguard once **sfg** is trained (~L40); before that **Voidwalker** (solo leveling
  survivability). Implement via the existing pet-strategy mechanism so a user can override.
- **Legionnaires:** Imp legionnaires (ranged, focus-fire — this is the model's 22% "imp-legionnaire" line).
- **Greater demons / es-permanent line:** OUT of the default build (see Open Decision #2). bor+es are the
  cut points precisely because this build is the legionnaire line.
- **Warlock-tank (VW anchor + Suffering):** out of scope unless opted in (Open Decision #3).

---

## 6. Low-level degradation ladder

| Levels | Rotation |
|---|---|
| 10–19 | Voidwalker + Corruption + **Summon Wild Imps** + Shadow Bolt. No shard engine yet (Drain Soul for shards). |
| 20–39 | + Soul Harvest economy, 1st legionnaire (imp), Fel Corruption Corruption-maintenance, Savage Instincts. |
| 40–49 | Anchor → Felguard (sfg), 2nd legionnaire, Vicious Pact. |
| 50–69 | + **Demonic Empowerment** on CD (spine amplifies), Wrath/Grim/Fervent. |
| 70–74 | + 3rd legionnaire (Legion Commander) → full army; Command Demon in the kit. |
| 75–80 | + **Doombrand** → the full pool→brand→command loop. |

---

## 7. Decisions (RULED by user 2026-08-20 — locked)

1. **Random-bot demonology probability** — **STOCK distribution** (no bias). ✅
2. **Greater demons / Eternal Servitude line** — **NO. Legionnaire line only.** es+bor stay cut; the
   71-pt build above is final. ✅
3. **Warlock-tank (Voidwalker anchor) bot support** — **OUT of scope** (ST DPS only; VW remains the
   low-level leveling anchor before Felguard). ✅
4. **PvP/battleground behavior** — **OUT of scope**. ✅
5. **Bot legionnaire-cap conf knob** — **uncapped**; add a knob only if Phase-5 perf demands it. ✅
6. **Anchor comp** — **Felguard anchor + 3 Imp legionnaires** (Felguard once sfg ~L40; Voidwalker
   before). No Succubus → `fb` stays a cut. ✅
7. **`CommandBrandAlignWindowMs`** — default **4000 ms**, conf-tunable. ✅

All decisions match the analysis above — **no build/rotation changes required.** Phase 0 complete.

---

*Phase 0 signed off. Next: Phase 1 — `tools/gen_playerbot_spec_links.py` (spec-link generator) on the go-ahead.*

---

## 8. Phase 1 outcome (talent plumbing) — DONE, static

`tools/gen_playerbot_spec_links.py` reads the shipped `dist/dbc/Talent.dbc` (validated against the
authoring YAML: 36 talents, every (row,col)/rank aligned, tab 303 = warlock/tabpage 1), validates the
71-pt build's prereqs + tier gates + max ranks + totals at **every** one of the 71 leveling points, and
emits positional `PremadeSpecLink.9.1.<10..80>` strings (digit = target rank, (Row,Col) order — exactly
how `ParseTempTalentsOrder` sorts and `InitTalentsBySpecNo` applies). Each breakpoint sums to exactly
`level-9` points → zero truncation. Output: `conf/playerbots-demonology.conf.fragment` (71 links + 5
demo-dip neutralizations; the only *active* one was `9.2.80` destro-pve). Demo = spec index 1 (34% stock
roll). `tools/playerbot_talent_check.sql` is the crash-loop guard. **Live dry-run deferred to Phase 5**
(installer owns the conf merge).

## 9. Phase 2 outcome (spell knowledge) — VERIFIED, no code change

The whole spell-knowledge chain works by construction on this commit:

1. **Trainer baselines reach bots.** Summon Wild Imps (`290001`, ReqLevel 10) and Demonic Empowerment
   (`290000`, ReqLevel 50) are on warlock class trainers 31/32 (`33_baseline_trainer_spells.sql`). The
   factory's `InitAvailableSpells` (PlayerbotFactory.cpp:3199) teaches *every* class-trainer spell that
   passes `CanTeachSpell` (level/skill/prereq — no gold gate for bots), so bots learn them at the right
   level exactly like Shadow Bolt / pet summons.
2. **Talent-derived spells reconcile automatically.** `Player::LearnTalent` (Player.cpp:14048) fires
   `OnPlayerLearnTalents` at its last line (14181) **unconditionally on success** — including the
   factory's `bot->LearnTalent()` calls. The rework's hook (CommandPool.cpp:912) runs
   `ReconcileDemonSpells` immediately, syncing Command Demon (on Expanded Command → our `ec=1`),
   legionnaire summons (on `command && HasSpell(pet-summon)`), and Doombrand (on `gwd`, from L60 — its
   tier-11 gate is 50 spent points, which a level-60 warlock affords). The factory
   runs `InitAvailableSpells` (line 674) **before** `InitTalentsTree` (683), so pet-summon prereqs are
   already known when the reconcile fires; `OnPlayerLogin` reconcile (CommandPool.cpp:883) is a backstop
   that also heals live bots. No new hook needed — the plan's preferred reconciliation hook already exists.
3. **Name collisions.** Playerbots resolves a spell *by name* against the **bot's own spellbook**
   (`SpellIdValue::Calculate`, highest-rank wins). Of our rotation spells only **"Demonic Empowerment"**
   shares a name with a stock spell (`47193`) — but `47193` is unreachable by a rework demo bot (its old
   demo talent is gone, it's not on any trainer, playerbots doesn't hardcode it), so the name resolves
   uniquely to `290000` in practice. **Forward requirement (Phase 4): ID-pin all custom-spell casts**
   (`290000/290001/290013/290014` + legionnaires) rather than name-resolve, so the rotation is immune to
   any future collision — per the plan's standing constraint.

**Deferred to live validation (Phase 5):** `.playerbots bot add` a demo bot, dump its spellbook, confirm
every rotation spell present at the right level and a hand-issued cast of each custom spell succeeds.

## 10. Phase 3-4 outcome (behavior) — BUILT, compiles+links

All playerbots changes are in `playerbots-patches/0001-demonology-integration.patch` (9 files, vs pin
`085e127e`); the rework's side of the bridge is `Demonology_BotLegionStatus` in `src/CommandPool.cpp`.
Every behavior is gated on `IsReworkDemonology()` (warlock + demo tab + spell 290013 exists) and the
cross-module call is a weak symbol, so a stock server is byte-for-byte unaffected. See
`playerbots-patches/MANIFEST.md` for the file-by-file breakdown.

- **Non-combat:** shard-destroy neutered (Path B); anchor = Felguard→Voidwalker→Imp; Imp legionnaire
  pool fill/refill via the pool bridge; Fel Armor already handled by the stock armor fall-through.
- **Combat:** ID-pinned casts (`CastReworkSpellAction`) for 290000/290001/290013/290014; rotation
  Doombrand → Command Demon (brand-align hold) → Demonic Empowerment → Wild Imps (above reserve) →
  Corruption → Life Tap; fillers Shadow Bolt → Shoot; `meta melee` dropped. Thresholds in conf.

## 11. Phase 5 — packaging DONE; live validation is the remaining step

**Packaged & self-tested:** `install_playerbots.sh` (dry-run default, `--apply`/`--revert`, `git apply
--check` drift-refusal, idempotent, comments out superseded stock keys since AzerothCore config is
FIRST-WINS on duplicates, **byte-for-byte revert verified**); conf fragment regenerated with the combat
tuning keys; README updated; worldserver builds clean with the patch applied.

**Live validation runbook** (needs a rebuild + restart of the live server — hand-run):
1. `./install_playerbots.sh --apply` then `cd /home/leo/wow/build && make -j$(nproc) worldserver && make install`; restart worldserver.
2. **Talent crash-safety (do FIRST, one bot):** add a single L80 demo bot, then run `tools/playerbot_talent_check.sql` (query A returns 0 rows). Restart once more; server must boot clean (no `character_talent` crash-loop).
3. **Spec:** in-game inspect the L80 bot → matches the §1 71-pt build; a L30 bot uses the level-30 breakpoint.
4. **Spell knowledge:** `.playerbots bot add`, dump spellbook → Wild Imps (≥10), Demonic Empowerment (≥50), Command Demon (with `ec`), Doombrand (≥L60), Imp legionnaire present; hand-cast each custom spell succeeds.
   **Per level cap:** at cap 60 the full loop (Command + Empowerment + Doombrand + Wild Imps + 3 legionnaires + Felguard) is complete at L60 (51 pts); caps 70/80 add throughput amplifiers (wotl/cotp/op/re/se) on top. Below L60 the rotation degrades by known-spell gates: no Doombrand until L60, no Command Demon until `ec` (~L23), no Demonic Empowerment until L50 — each trigger checks `HasSpell` so absent abilities are simply skipped.
5. **Non-combat:** out of combat the pool fills to cap within ~30s, resummons a killed legionnaire, keeps Fel Armor, shard count grows over a grind (never destroyed).
6. **Combat (dummy at 80):** Corruption uptime >95%; a brand every ~20-25s; ≥80% of Command presses land in a brand window when CDs align; Empowerment on CD/in-window; shards never hit 0 mid-fight; **zero melee approaches**.
7. **Fleet:** 20+ random demo bots; watch worldserver perf + error log for name-resolution / dead-trigger spam.

## 12. Live validation — VALIDATED, seven as-built fixes

Live testing (test bot "Nerthen", L60 demo, 2026-08-21) confirmed the full loop end-to-end — correct
build, Felguard anchor, Imp legionnaires filling, Wild Imps + Demonic Empowerment, Doombrand/Command
rotation. Getting there surfaced seven integration bugs that offline validation did not catch; all are
fixed in `playerbots-patches/0001-demonology-integration.patch` and gated on rework presence.

1. **First-spawn strategy timing.** `PlayerbotMgr::OnBotLogin` runs `ResetStrategies()` (which applies
   the spec-gated demo wiring) *before* the addclass `factory.Randomize()` that applies the spec — so a
   fresh addclass bot got stock strategies. Fix: call `ResetStrategies()` again after the respec. (The
   random fleet is unaffected — it logs in with persisted talents.)
2. **Trainer baselines (Wild Imps 290001 / Demonic Empowerment 290000) not learned.** Playerbots'
   trainer walk doesn't deliver these custom spells to bots, and granting them mid-`Randomize` doesn't
   persist. Fix: grant at login in `OnBotLogin` (like the rework's own reconcile), where `learnSpell`
   sticks.
3. **Pet-strategy sibling eviction (the "no pet summon" bug).** Playerbots pet strategies live in a
   mutually-exclusive `NamedObjectContext(supportsSiblings=true)` context, and `Engine::addStrategy`
   removes a strategy's siblings. Registering both `legion anchor` and `legion pool` there meant adding
   the pool evicted the anchor before it initialized. Fix: **one** pet strategy — `SummonLegionAnchor`
   drives both the anchor and the legion-pool fill; AiFactory adds only `"legion anchor"`.
4. **Anchor summon.** `CastLegionAnchorAction` casts Felguard(30146)->Voidwalker(697)->Imp(688) by id.
5. **Legionnaires only filled out of combat.** The pool-fill was a non-combat trigger, but bots earn
   shards *in* combat (Soul Harvest). Fix: also drive `legion pool not full -> summon imp legionnaire`
   from the combat rotation (relevance 21.5, above Wild Imps, below brand/command).
6. **Shard bootstrap.** A fresh command-spine bot can't afford anchor + legionnaires before combat
   earns shards. Fix: top up to a floor of 10 soul shards at login for warlock bots with Expanded
   Command (talent 290922).
7. **Legionnaire summon rejected with `SPELL_FAILED_TARGET_IS_PLAYER` (117).** The legionnaire summons
   (290002-6, cloned from the pet-targeting 47193) reject an explicit *player* target, and playerbots'
   `CanCastSpell`/`CastSpell(bot)` sets exactly that. A real player casts them targetless. Fix: cast
   **targetless** — `bot->CastSpell((Unit*)nullptr, 290002, false)` (implicit `unit_caster`,
   non-triggered so the soul-shard reagent is still consumed). **General lesson: self-cast spells
   cloned from pet-target spells must be cast targetless from bot code.**

**Diagnostic method (what worked):** instrument the exact decision point and log to `worldserver.log`
(readable directly), rather than infer. Playerbots' own `LogAction`/`CanCastSpell` failure reasons are
`LOG_DEBUG` (off by default; too noisy at fleet scale), so a targeted, one-bot-gated `LOG_INFO` at the
failing call — including the raw `SpellCastResult` code — pinpointed each cause. All such logging was
removed for the final patch.
