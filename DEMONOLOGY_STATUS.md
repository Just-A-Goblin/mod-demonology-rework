# Demonology Rework — Current State & Redesign Brief

_Living status document. Snapshot as of 2026-08-21. Supersedes the stale ✅/⬜ column in
`DEMONOLOGY_DESIGN_V2.md §4` for tracking purposes — that doc remains the governing design._

## 0. Changelog

### 2026-08-21 — polish pass (built, deployed, live-verified)
- **Command Demon** — cost now follows the standard spell formula: **2 Soul Shards as a native reagent**
  (item 6265 ×2, shown in the tooltip + consumed by the core) **+ 10% base mana** (DBC `ManaCostPct`, up
  from 5%). Removed the old C++ item charge and the `Demonology.CommandDemon.ShardCost` config knob
  (mirrors how the legionnaire summons already work).
- **Summon Wild Imps** — killed the "invisible projectile" summon delay. The 686 (Shadow Bolt) clone
  carried a projectile `Speed` (~21 yd/s) that deferred the summon until the missile "landed"; the visual
  was already instant, so it looked like a phantom bolt. Fixed by zeroing the DBC `Speed` field via a new
  `speed:` authoring field added to SpellForge (`model/spell.py` + `compile/spells.py`).
- **Shard economy (glut fix)** — `Demonology.SoulHarvest.InternalCooldownMs` **1000 → 4000**. The ICD is
  the income ceiling (a full legion fills nearly every window), so this cuts sustained shard income ~4×
  (~15/min) to roughly match what the war machine spends. Per-rank chance unchanged.
- **Summon "ding" removed** — Wild Imps, legionnaires, and greater demons no longer play the client
  level-up animation on summon (`SetLevel(owner->GetLevel(), false)` — `showLevelChange=false`).
- **Wrath of the Legion** — description now states "Each additional imp costs 5% of your base mana to
  manifest" (matches `WrathOfTheLegion.ManaCostPct`).
- **Legionnaire summon visual** — no longer flashes the Demonic Empowerment over-head skull. All 5
  legionnaire summons override `SpellVisualID` (47193's `13422`) with `visual: {copy_from: 697}` = `4054`
  (Summon Voidwalker's purple summon flash). `SpellVisualID` is client-only, so this shipped via the
  client patch with no server restart. Command Demon + Infernal/Doomguard summons still inherit the
  skull (not flagged; easy to change the same way).
- **Summon Wild Imps — 1.5s cast time** (`cast_time: 1.5s`, `CastingTimeIndex 16`). A deliberate
  **mobility nerf**: Demonology's throughput was too high for a spec that could summon its whole pack
  while kiting; the pack now roots you like a real summon. `Speed=0` is preserved, so the imps still
  appear the instant the cast completes. Cast time is server-enforced, so this needed the server DBC +
  a restart (plus the client patch for the cast bar).
- **Wild Imps despawn on player death** — Wild Imps are plain `SummonCreature` guardians (not in
  `m_Controlled`), so the core never dismissed them; the pack kept fighting after you died. Added
  `Demonology::DespawnWildImps(owner)` (SummonSpells.cpp, grid-search owned `NPC_WILD_IMP` + clears the
  wotl chain-budget map) wired to `OnPlayerJustDied` in `demonology_pool_playerscript`. Scoped to Wild
  Imps only — the persistent legionnaires + greater demon are the standing army and survive death by design.

## 1. Where the module stands

The module is **feature-complete against the DESIGN_V2 build order (Phases 0–7)**: the full 36-node
talent tree renders, and Command Demon, Doombrand, the Empowerment spine, greater demons, per-type
pet signatures, and a real balance pass are all in and playtested. Deploy is stable (server +
DBC + client MPQ). Remaining work is a short list: **3 talents to redesign, 2 partials to finish,
1 inconsistency to fix, and ongoing tuning.**

Balance tooling lives in `docs/`: `balance_model.py` (spec-vs-spec), `pet_damage.py`,
`legionnaire_dps.py`, `anchor_dps.py`, `slot_comparison.py`.

---

## 2. Full talent audit (all 36 nodes)

Legend: ✅ implemented · 🟡 partial · ❌ not implemented (dead node) · 🔧 redesign needed

| Tier | Talent | key | Status | Notes |
|---|---|---|---|---|
| T1 | Improved Legion | il | 🟡 | Shard-cost reduction on Wild Imps **works**; the rank-1 **summon cast-time** reduction is **inert** (parked — see §5). |
| T1 | Fel Conditioning | fc | ✅ | Demon health %. |
| T1 | Soul Harvest | sh | ✅ | Shard generation on demon damage (reads `SPELL_SOUL_HARVEST_R1..3`, not a TALENT_ marker). |
| T1 | Cursed Vitality | cv | ✅ | Demon stamina→health + owner-stamina half. |
| T2 | Fel Corruption | rc | ✅ | **REDESIGNED & built (Addendum A.2):** your Corruption's periodic damage counts as demon damage for Soul Harvest + Doombrand at 33/66/100% (Doombrand at half weight). Allowlist helper `LegionEconomy::QualifyPlayerPeriodic` — no other demon-damage consumer fires. Marker 290909. |
| T2 | Fel Armory | fa | ✅ | +demon damage under Fel Armor. |
| T2 | Cruel Master | cm | ✅ | Demon crits accelerate Soul Harvest. |
| T3 | Pactbound Fury | pf | ✅ | Real demon crits (melee + spell). |
| T3 | Fel Blood | fb | ✅ | Lash of Pain +dmg + Corruption refresh on demon killing blow. Boosts the Succubus Command echo (290502) **and** its signature (290515) — fixed with the Addendum A build. |
| T3 | Expanded Command | ec | ✅ | +1 command slot. |
| T3 | Vital Conduit | vc | ✅ | **REDESIGNED & built (Addendum A.1):** Life Tap also heals your commanded demons for 50/100% of the health sacrificed, split among them (overheal allowed). Heal spell 290516; no longer a dead prereq tax. |
| T4 | Savage Instincts | si | 🟡 | Demon melee attack-speed **works**; the **caster cast-speed half** is **inert** (parked — see §5). |
| T4 | Shadowflame Legion | sl | ✅ | Empowerment absorb shield (redesigned off the cut Hand of Gul'dan). |
| T4 | Improved Wild Imps | iwi | ✅ | +imp duration + Firebolt 2nd-target. |
| T5 | Expanded Command II | ec2 | ✅ | +1 command slot. |
| T5 | Vicious Pact | vp | ✅ | SP→demon AP/SP. |
| T5 | Blood Tithe | bt | ✅ | Demon damage heals owner (2× at 3+ demons). |
| T5 | Warded Legion | wl | ✅ | Demon spell-resist + rank-2 Fear/Charm/Poly immunity. |
| T6 | Demonic Rebirth | dr | ✅ | Instant resummon on demon death. |
| T6 | Dark Command | dc | ✅ | Command Demon CD reduction + responder haste. |
| T6 | Unholy Vigor | uv | ✅ | Empowerment duration. |
| T7 | Summon Felguard | sfg | ✅ | Gates Felguard pet + legionnaire. |
| T7 | Wrath of the Legion | wotl | ✅ | Firebolt chance to spawn an extra Wild Imp (chain-capped). |
| T7 | Grim Bargain | gb | ✅ | Demon↔owner damage proc synergy (redesigned off the cut Grimoire duplicate). |
| T7 | Fervent Standard | fs | ✅ | **REDESIGNED & built (Addendum A.3):** your Demonic Circle is the legion's banner — within 20yd, you and your demons deal +4/8% damage and your demons take −5/10% damage. Direct range checks vs the circle GameObject (no cache). Marker 290954; cosmetic icon 290517. |
| T8 | Overlord's Presence | op | ✅ | Per commanded demon: owner +HP/haste. |
| T8 | Fel Conduit | fcd | ✅ | Demon attacks proc instant/free Shadow Bolt (3 charges). |
| T8 | Riftwalker | rw | ✅ | Demonic Circle: Teleport warps demons + speed (redesigned off the cut Rend Veil). |
| T8 | Cruelty of the Pit | cotp | ✅ | Empowered demons +damage. |
| T9 | Eternal Servitude | es | ✅ | Permanent Infernal/Doomguard + 60s CD. |
| T9 | Ruinous Empowerment | re | ✅ | Empowerment leech + no-expire chance. |
| T10 | Beacon of Ruin | bor | ✅ | Infernal/Doomguard +15/30% dmg + summon CD −20/40%. **Moved to T10/row9 and gated behind Eternal Servitude** so it's no longer a stray inert node in a legionnaire build — you only take it once you've committed to permanent greater demons. |
| T10 | Legion Commander | lc | ✅ | +1 command slot. |
| T10 | Supreme Empowerment | se | ✅ | Empowerment affects temp demons + longer. |
| T10 | Bound by Blood | bbb | ✅ | On demon death: survivor buff + shard refund. |
| T11 | Grand Warlock's Design | gwd | ✅ | Grants Doombrand. |

**Summary:** 36 ✅ · 0 🟡 · 0 ❌🔧 (as of 2026-08-20). All 36 nodes fully implemented. The former
dead nodes (rc→Fel Corruption, vc, fs→Fervent Standard) are live per Addendum A; il is a single-effect
mana-efficiency node (Summon Wild Imps mana cost + SPELLMOD reduction); si is complete — its caster half
scales the demon AI recast cadence (+ anchor-pet cast-speed). A balance pass (FLATTEN) has been applied
(see docs/balance_model.py + the balance-model memory). Remaining work is validation only: real level
60/80 combat parses to replace the ±20% expected-value model.

---

## 3. Beacon of Ruin — it works, but it's conditional

> **RESOLVED (2026-08-20):** bor was moved to **T10/row 9** and now **requires Eternal Servitude**,
> so it can no longer be taken in a build that never fields a greater demon. The section below is
> retained for history; the "make the dependency obvious" recommendation is now enforced by tree
> position + prereq rather than tooltip alone.

bor **is** implemented (`BeaconOfRuinDamage` in the demon-damage hook, `BeaconOfRuinCdReduction`
in the greater-demon summon AfterCast). It gives Infernal/Doomguard +15/30% damage and cuts their
summon cooldown 20/40%. **But it only does anything when you have a greater demon summoned**, which
requires **Eternal Servitude** (a T9 talent) plus casting Summon Infernal/Doomguard. In a pure
legionnaire build, bor is inert — which is almost certainly why it "seems to do nothing." This is
working as designed (it's the greater-demon build's damage node), but the dependency is worth
making obvious in the tooltip so players don't take it without es.

---

## 4. Talents that must be rewritten (dead nodes)

> **SUPERSEDED (2026-08-20) by `ADDENDUM_A_DEAD_NODE_REDESIGNS.md`.** All three nodes below are
> now built and deployed: rc → **Fel Corruption** (A.2), vc → **Vital Conduit / Life Tap heal**
> (A.1), fs → **Fervent Standard** (A.3). The design exploration below is kept for history only;
> the shipped effects are the ones in §2 and the addendum.

Each needs a **design decision** (what it becomes), then a small build. All three currently let a
player spend points for zero effect.

### 4.1 Rapid Conjuration (rc) — T2, 3 ranks, prereq il@2
- **Original:** summon cast −1.5/3/4.5s; R3 cast while moving.
- **Why cut:** legionnaires already summon in ~1.5–2s; clean cast-time reduction needs a
  `SPELLMOD_CASTING_TIME` aura our `HasTalent`-only dummy markers can't carry; and the spec already
  trends strong, so raw throughput isn't wanted.
- **Redesign options:**
  - **(a) Summon economy** — legionnaire/Wild Imp summons have a chance to refund a Soul Shard, or
    cost less. Hook-friendly, fits Path B, low power creep.
  - **(b) Instant-summon proc** — chance your next legionnaire summon is instant + free (a proc
    buff, same mechanism as Fel Conduit). Keeps the "rapid conjuration" fantasy without SPELLMOD-ing
    the cast bar.
  - **(c) Fresh-summon rally** — a newly summoned demon arrives with a short +damage/+haste buff
    (rewards active resummoning without permanent throughput).
- **Recommendation:** (b) — it delivers the node's fantasy and reuses the proven Fel Conduit proc
  pattern.

### 4.2 Vital Conduit (vc) — T3, 2 ranks  *(also Blood Tithe's prereq — priority)*
- **Original:** Health Funnel +20/40% efficiency, no self-damage.
- **Why cut:** too strong and **off-flavor** — a free channeled heal loses the warlock's
  life/soul-for-power identity.
- **Redesign options (keep the life-trade):**
  - **(a) Funnel → shield** — Health Funnel also grants the demon (or you) an absorb shield equal to
    a % of the health funneled. You still spend your life; the payoff is defensive, not a free heal.
  - **(b) Soul sustain** — a demon's death heals you for a % of your health (life reclaimed from the
    fallen), or spending a shard heals your demons.
  - **(c) Fel pact** — Life Tap also grants your demons +damage for a few seconds (trade your life
    for the legion's fury).
- **Recommendation:** (a) — smallest, keeps the Health Funnel channel, adds a defensive payoff that
  reads as a *trade*. **Must ship something here** so bt's prereq isn't a dead tax.

### 4.3 Fervent Standard (fs) — T7, 2 ranks
- **Original:** boosted **Legion Standard**, a planted-banner ability that was **banked (never
  built)**. So fs points at an ability that doesn't exist.
- **Redesign options (self-contained, no new creature):**
  - **(a) Rallying presence (revived Legion Aura)** — while you command ≥N demons, you and your
    party gain a small aura (+5/10% damage or haste). This is the Legion Aura that was cut from gwd;
    "Fervent Standard" is a natural home for it. _(Note: it was cut once as unnecessary — reconsider
    if you want a party-buff identity here.)_
  - **(b) Hold the line** — your commanded demons deal +5/10% damage while near you / stationary.
  - **(c) Standard-bearer** — your legionnaire summons plant a short-lived buff zone (uses the
    existing owner-aura/`OnPoolChanged` plumbing, no new creature).
- **Recommendation:** (a) or (b) — both are pure passives on existing hooks.

---

## 5. Partial implementations (finish or formally park)

- **Improved Legion (il) — cast-time half:** the config key `ImprovedLegion.CastReductionMs` is read
  but never applied (same SPELLMOD blocker as rc). The **shard-cost** half works. Decide: redesign
  the cast-time half like rc, or drop it and make il a single-effect node.
- **Savage Instincts (si) — caster half:** melee attack-speed works; demon **cast-speed** does
  nothing because demons auto-cast on fixed AI cooldowns (2s/3s). To make it real, scale the AI
  recast cooldown by the si haste; otherwise formally drop the caster-half claim.

---

## 6. Known inconsistency to fix

**Fel Blood (fb) does not boost the Succubus legionnaire's new signature.** fb's damage bonus keys
off `SPELL_EMPOWERED_LASH_OF_PAIN` (290502) and the vanilla Lash chain (7814). The Succubus
legionnaire now casts **`succubus_lash` (290515)**, a separate spell not in the 7814 chain, so fb
skips it. **Fix:** add 290515 to the fb check in `demon_damage::ModifySpellDamageTaken` (one line).

---

## 7. Balance state (for context)

Recent tuning (all in conf, `docs/*.py` models):
- **Spec balance:** Demo single-target was +90–160% vs Aff/Destro; a balance pass (WildImp coef,
  Doom Bolt SP-weight, Doombrand) brought it to ~+25–32% ST while keeping the intended AoE-weak
  shape (Aff/Destro win multi-target via Seed/Rain of Fire).
- **Pets:** per-type legionnaire signatures added; the Imp legionnaire (was worst) bumped to the
  Succubus/Felhunter tier via a **decoupled** `ImpLegionnaire.SPCoefficient`; spec-agnostic
  **anchor Imp** buff.
- **Greater demons differentiated:** **Doomguard = single-target nuke** (SP-weighted Doom Bolt,
  token Doom Blast), **Infernal = AoE/tank** (periodic fire nova). Level-60 overshoot fixed via
  SP-weighting the flat bases.

Open tuning questions: fold Empowerment/Doombrand into the models (they're baseline-only); firm up
the ±30% anchor-pet estimates with a real parse; Command Demon off-GCD? (revisit — Doombrand is
playable now); taper on/off default.

---

## 8. What remains (prioritized)

1. **Redesign the 3 dead nodes** — rc, vc, fs (vc first: it's a prereq). Each: pick an effect above,
   build (~small), deploy.
2. **Fix the Fel Blood / Succubus-signature inconsistency** (one line).
3. **Finish or formally park the 2 partials** — il cast-time, si caster half.
4. **Clarify Beacon of Ruin's tooltip** (or accept it as the greater-demon build's node).
5. **Ongoing tuning** — playtest the freshly-tuned pets/greater demons; refine coefficients.

After 1–3, every node in the tree does something and no talent points on an absent ability.
