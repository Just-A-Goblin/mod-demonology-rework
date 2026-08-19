# Demonology Rework — Design & Implementation Doc (v2)

**Module:** `mod-demonology-rework` (AzerothCore 3.3.5a, solo server + playerbots)
**As of:** 2026-08-18
**Supersedes:** DEMONOLOGY_IMPLEMENTATION_STATUS.md (v1)
**Audience:** implementation by Claude Code. Every ⬜ item is specified to buildable
detail; open questions are quarantined in §12 and nothing in §12 blocks Phases 1–6.

Legend: ✅ implemented & in-game · 🟡 partial · ⬜ approved design, not implemented ·
🔶 pending a decision in §12.

---

## 1. Design principles (settled — do not re-litigate in implementation)

1. **Spec identity:** Affliction = DoTs, Destruction = direct nukes, **Demonology =
   army building/commanding**. Anything pulling Demo toward caster gameplay is out.
2. **Baseline-spell budget:** max two new baseline spells. **Spent: Command Demon
   (`290013`). Banked: one slot** (Legion Standard is the standing candidate, §12.1).
   Talent-granted spells (Doombrand, Summon Felguard, Soul Harvest) don't count.
3. **Command rules:** one button, one decision (which demon is anchor); signatures
   execute once from the anchor; echoes are non-decision; **no breakable CC anywhere in
   the command system** — hard CC (Seduction) is a deliberate manual pet cast only.
4. **Shard economy = Path B (inverted):** shards fuel *actives*, not summons-as-such.
   "The army's damage fuels your commands; you spend it back on the legion."
5. **Cut list (abilities NOT being built):** Metamorphosis, Hand of Gul'dan, Fel Lash,
   Rend Veil, Grimoire-of-Synergy-as-duplicate-demon, free/instant-first-summon.
   Every talent that referenced them has a redesign in §4.
6. **Tuning lives in conf.** No hardcoded balance numbers in new code. The live `.conf`
   is a Phase-0 prerequisite (§9).
7. **Ship weak, buff later.** Especially Doombrand and echo damage — a nerfed capstone
   reads terribly.

---

## 2. System backbone

### 2.1 Existing systems (✅, verified in-game — unchanged from v1)

| System | Notes |
|---|---|
| Command Pool | One pool/warlock; anchor (slot 0) + up to N legionnaires. |
| Slot cap from talents | Base 0; ec/ec2/lc each +1 (max 3). |
| Legionnaire roster | Imp/Felguard/Succubus/Felhound/Voidwalker, mixed comp, evict-oldest. |
| Mirroring AI | Inherits anchor react-state + target; CC-aware (never breaks anchor Seduce or owner Fear/Poly). |
| Unified defensive response | `FindLegionThreat` shared by mirror + auto-assist. |
| Stat inheritance (PetScaling) | %owner HP; SP-derived melee; live re-apply. All owned demons. |
| Demon damage scaling hook | Talent multipliers applied at hit time to any warlock-owned demon. **Doombrand's accumulator and Pactbound Fury's crit roll both live here (§5, §4).** |
| Shard economy (Soul Harvest) | Per-player ICD generation on demon damage. Retained under Path B. |
| Hybrid-learn | Talent/known-pet → summon spells in Demonology tab; removed on respec. |
| Greater demons | Infernal (2 slots) / Doomguard (3 slots); SP-scaled Doom Bolt/Blast. |
| Persistence | Legionnaires survive logout (health%); stash on death/mount. Greater demons do NOT persist (see §11). |
| Demonic Empowerment | Buffs pet + legionnaires + wild imps + active greater demon. |
| `.legion` GM tooling | pool/dumpstats/recruit/dismiss/summon/shards. Extensions in §5.7, §6.6. |
| Client tree render | 36 nodes via `DemoTalentFix` addon. |

### 2.2 New systems (⬜, this doc is the spec)

| System | Spec | Consumed by |
|---|---|---|
| **Command Demon** (dispatch: anchor signature + per-type echoes + greater-demon responses) | §5 | dc talent; signature identity |
| **Doombrand** (brand → army charges sigil → detonation) | §6 | gwd capstone |
| **Path B shard costs** (actives spend shards) | §7 | il, cm, bbb re-reads |
| **`OnPoolChanged` owner-aura driver** | §8.1 | op, gwd's Legion Aura rider, cv owner half |
| **Demon-death event hook** | §8.2 | dr, bbb |
| **Signature autocast wiring** | §8.3 | per-type demon feel; future signature talents |
| **Enthralling Presence** (succubus signature) | §5.5 | Command Demon |

---

## 3. Spell & creature ID registry

**Player spells (29001x block):**
- `290000` Demonic Empowerment · `290001` Summon Wild Imps
- `290002–290006` Summon Legionnaire Imp/Felguard/Succubus/Felhound/Voidwalker
- `290007` Summon Infernal · `290008` Summon Doomguard
- `290010–290012` Soul Harvest R1–3
- `290013` **Command Demon** ⬜ · `290014` **Doombrand** ⬜ (granted by gwd)

**Internal / ability spells (2905xx block — keep NEW internals here; 2909xx is dense
with talent markers):**
- `290500` Demonic Empowerment buff (existing)
- `290501` Enthralling Presence (enemy debuff) ⬜
- `290502` Empowered Lash of Pain (succubus legionnaire echo) ⬜
- `290503` Infernal Command Pulse ⬜
- `290504` Doombrand debuff ⬜ · `290505` Doombrand detonation ⬜
- `290506` Voidwalker Command Shield (owner absorb — VW signature) ⬜
- `290507` Voidwalker Consume Shadows (self-shield — VW echo) ⬜

**Pet/guardian abilities (2909xx, existing):** `290900` Wild Imp Firebolt ·
`290901` Doom Bolt · `40878` Doom Blast (capped).

**Vanilla pet abilities reused by Command Demon** (rank-1 chain anchors — resolve top
ranks from `spell_dbc` at startup, never hardcode top-rank IDs): Intercept `30151`,
Cleave `30213`, Spell Lock `19244`, Devour Magic (chain from `19505`), Suffering `17735`,
Sacrifice `7812` (modeled, owner-shield variant), Lash of Pain `7814`, Firebolt `3110`,
Consume Shadows `17767` (flavor ref for VW echo absorb), Seduction `6358` (manual only).

**Creatures (existing):** Wild Imp `600000`; legionnaires reuse base pet entries (Imp
416, Felhunter 417, Voidwalker 1860, Succubus 1863, Felguard 17252); Infernal 89,
Doomguard 11859.

**Gating (existing):** Felguard pet `30146` behind sfg talent; basic-pet legionnaire
versions unlock at known-pet + Expanded Command.

---

## 4. Talent tree — 36 nodes, updated statuses & redesigns

Tree: 11 tiers, layout `[4,3,4,3,4,3,4,4,3,3,1]`, 78 points, tab 303. Marker = rank-1
spell read via `HasTalent`. **Redesigned effects below are the approved versions** —
where v1 said "blocked," the new effect is normative.

| Tier | Talent | key | Ranks | Marker | Status | Effect (current design) |
|---|---|---|---|---|---|---|
| T1 | Fel Conditioning | fc | 5/10/15% | 290904 | ✅ | Demon health %. |
| T1 | Improved Legion | il | 1 | 290902 | ⬜ | Legionnaire summons cast 0.5s faster; **Summon Wild Imps costs 1 less shard** (Path B rework — resummon cost can't absorb a reduction without hitting 0). |
| T1 | Soul Harvest | sh | R1–3 | — | ✅ | Shard generation on demon damage. Retained (Path B). |
| T1 | Cursed Vitality | cv | 3/6% | 290907 | 🟡 | Demon stamina→health ✅; owner +3/6% stamina ⬜ (owner-aura, §8.1). |
| T2 | Rapid Conjuration | rc | 1/2/3 | 290909 | ⬜ | Summon cast −1.5/3/4.5s; R3 castable moving. |
| T2 | Cruel Master | cm | 1/2 | 290915 | ⬜ | Demon crits double Soul Harvest proc chance / reduce its ICD. Real throughput under Path B. |
| T2 | Fel Armory | fa | 5/10/15% | 290912 | ✅ | +demon damage under Fel Armor. |
| T3 | Expanded Command | ec | +1 | 290922 | ✅ | +1 slot. |
| T3 | Vital Conduit | vc | 20/40% | 290923 | ⬜ | **Promoted from blocked:** hook the existing Health Funnel chain (`755`) via Spell/AuraScript — +efficiency, no self-damage while channeling. No new ability needed. |
| T3 | Pactbound Fury | pf | 2/4/6% | 290917 | ⬜ | **Solved in the damage hook:** roll talent crit chance at hit time, multiply damage, flag the log entry as crit. No creature-crit setter needed. |
| T3 | Fel Blood | fb | 15/30% | 290920 | ⬜ | **Redesigned (Fel Lash cut):** succubus Lash of Pain (incl. `290502` echo) +15/30% damage; demon killing blows refresh your Corruption on a nearby target. |
| T4 | Improved Wild Imps | iwi | 5/10 | 290930 | ⬜ | +imp duration; Firebolt 10/20% chance to hit a 2nd target. |
| T4 | Shadowflame Legion | sl | 15/30% | 290928 | ⬜ | **Redesigned (HoG cut):** Demonic Empowerment also applies an absorb shield to affected demons (15/30% of the demon's max HP). Part of the Empowerment spine. |
| T4 | Savage Instincts | si | 4/8/12% | 290925 | 🟡 | Demon melee haste ✅; caster cast-speed half ⬜. |
| T5 | Expanded Command II | ec2 | +1 | 290932 | ✅ | +1 slot. |
| T5 | Blood Tithe | bt | 4/8% | 290936 | ⬜ | Demons heal you for % damage dealt (doubled at 3+ demons). |
| T5 | Warded Legion | wl | 9/18% | 290938 | ⬜ | Demon full spell resist chance; R2 Fear/Charm/Poly immunity. |
| T5 | Vicious Pact | vp | 8/16/24% | 290933 | ✅ | SP→demon AP + SP→demon spell power. |
| T6 | Demonic Rebirth | dr | 50/100% | 290940 | ⬜ | On demon death, chance to instantly resummon (60s ICD). Demon-death hook (§8.2). |
| T6 | Dark Command | dc | 3 ranks | 290942 | ⬜ | **Unblocked by Command Demon:** its CD −5/10/15s; each press grants responding demons a short haste buff. |
| T6 | Unholy Vigor | uv | 1/2/3 | 290945 | ⬜ | Demonic Empowerment +1/2/3s. |
| T7 | Summon Felguard | sfg | 1 | 290948 | ✅ | Gates Felguard pet + legionnaire. |
| T7 | Wrath of the Legion | wotl | 10/20/30% | 290949 | ⬜ | Firebolt chance to summon an extra wild imp. |
| T7 | Grim Bargain | gb | 6/12% | 290952 | ⬜ | **Redesigned (Grimoire-duplicate cut) as proc synergy:** demon damage has a chance to grant YOU +6/12% damage for 8s; your damage has a chance to grant your demons the same. Proc source = existing damage hook. |
| T7 | Fervent Standard | fs | 5/10 | 290954 | 🔶 | Pending Legion Standard decision (§12.1). |
| T8 | Overlord's Presence | op | 2/4/6% | 290956 | ⬜ | Per commanded demon: +owner max HP & haste. Driven by `OnPoolChanged` (§8.1). |
| T8 | Fel Conduit | fcd | 5/10% | 290959 | ⬜ | Demon attacks proc an instant/free Shadow Bolt (stacking). NOTE: these proc bolts are player damage — they do NOT charge Doombrand or Soul Harvest. |
| T8 | Riftwalker | rw | 1 | 290961 | 🔶 | **Proposed redesign (Rend Veil cut):** your Demonic Circle: Teleport also warps all commanded demons to your side and grants them +30% move speed for 6s. Uses existing baseline spell; approve in §12.2. |
| T8 | Cruelty of the Pit | cotp | 5/10/15% | 290962 | ⬜ | Empowered demons +damage (Empowerment spine). |
| T9 | Bound by Blood | bbb | 15/30% | 290974 | ⬜ | On demon death: others gain damage/haste; you regain a shard ("demon deaths fund your actives" under Path B). Demon-death hook (§8.2). |
| T9 | Beacon of Ruin | bor | 30/60% | 290969 | ⬜ | **Repurposed (Metamorphosis cut) as the greater-demon talent:** Infernal/Doomguard deal +30/60%... start lower in conf; suggest +15/30% damage and −20/40% summon CD. Natural neighbor to es. |
| T9 | Ruinous Empowerment | re | 7/14/20% | 290966 | ⬜ | Empowerment grants leech + no-expire chance (spine). |
| T10 | Legion Commander | lc | +1 | 290971 | ✅ | 3rd slot. |
| T10 | Eternal Servitude | es | 1 | 290965 | ✅ | Infernal/Doomguard permanent + 60s CD (req lc). |
| T10 | Supreme Empowerment | se | 3/6 | 290972 | ⬜ | Empowerment affects temp demons + lasts longer (temp part largely done; needs gating/duration). |
| T11 | Grand Warlock's Design | gwd | 1 | 290976 | ⬜ | **Reworked capstone:** grants **Doombrand** (§6) + Legion Aura rider (party +5% dmg/haste while ≥1 demon commanded; §8.1). Free-first-summon effect CUT (persistence made it vestigial). |

### Summary
- **✅ (9):** fc, sh, fa, ec, ec2, vp, sfg, lc, es
- **🟡 (2):** cv, si
- **⬜ approved & buildable (23):** il, rc, cm, vc, pf, fb, iwi, sl, bt, wl, dr, dc, uv,
  wotl, gb, op, fcd, cotp, bbb, bor, re, se, gwd
- **🔶 pending §12 (2):** fs (Legion Standard), rw (redesign approval)
- **🔒 (0)** — no talent is blocked on an unbuilt large ability anymore.

---

## 5. Command Demon — full spec

### 5.1 Player spell

| Field | Value |
|---|---|
| Spell | `290013`, baseline Demonology (spends spell-budget slot 1) |
| Cost | 1 Soul Shard · **Cooldown** 45s (conf) · instant, on GCD |
| Requires | Living, actionable anchor (fails if anchor dead/feared/stunned). Felguard/felhound/succubus/imp presses require a hostile target; voidwalker press does not. |

### 5.2 Anchor signature dispatch (fires once, anchor only)

| Anchor | Signature |
|---|---|
| Felguard | Intercept target, then Cleave. (Short charge stun allowed — unbreakable ≠ breakable CC.) |
| Felhound | Spell Lock target; if not casting, silence component still applies (never a wasted press). |
| Voidwalker | AoE taunt around anchor (Suffering) + Sacrifice-style absorb on the OWNER. |
| Succubus | **Enthralling Presence** (§5.5). Manual Seduction stays on her pet bar, untouched. |
| Imp | Volley: 3 rapid Firebolts. |

### 5.3 Legionnaire echoes (per own type, regardless of anchor; free; per-unit CD = Command CD)

| Legionnaire | Echo |
|---|---|
| Felguard | Cleave → anchor's target. |
| Felhound | Devour Magic: purge 1 buff from anchor's target; else cleanse 1 magic debuff (owner → anchor → self, first found). Deterministic list, no AI. |
| Voidwalker | Small instant self-shield (Consume Shadows-flavored). NOT a taunt (multiple taunts scatter aggro; anchor VW owns aggro). |
| Succubus | Empowered Lash of Pain `290502` → anchor's target. Carries extra weight (§5.6). |
| Imp | 2 Firebolts → anchor's target. |

### 5.4 Greater demon responses (bonus, not the reason to press)

Infernal: Command Pulse `290503` (hard immolation tick + small ground slam). Doomguard:
immediate Doom Bolt `290901`. No extra shard cost; keep each ≤ ~1 echo's value.

### 5.5 Enthralling Presence (`290501`)

- Center: **anchor's current target**; radius 8yd; duration 2.5s.
- Enemy debuff: −25–30% attack speed + −30% move speed. Register with
  `MECHANIC_SNARE`/`MECHANIC_DAZE` → boss/flag immunities apply to the control half free.
- Threat drop: −30–40% current threat for owner + every pooled demon — **imperative**,
  not an aura: iterate hostile refs, `ModifyThreatByPercent(-X)` at dispatch.
  (`SPELL_AURA_MOD_THREAT` modifies generated threat — wrong tool.) Works on bosses.
- **Skips units charmed by the owner** (never touches your manual Seduction).
- Requires hostile target.
- Reserved talent hook: Presence slow +15/30% & +1/2s, or R2 "affected enemies deal −X%
  to your demons."

### 5.6 Tuning rails

- Succubus press (echoes only) lands within ~15–20% of a felguard press (sig+echoes);
  `290502` is the lever; threat drop + slow make up the gap.
- Damage anchors: signature ≈ 40–50% of press value; echoes are the bulk in big pools.
- All values in conf.

### 5.7 Implementation hooks

1. SpellScript `290013`: validate anchor + target reqs, spend shard, call dispatcher.
2. Dispatcher (Command Pool code, beside `FindLegionThreat`): anchor entry → signature
   fn; iterate pool → echo fn per entry; greater-demon check → response. All targeting
   from the anchor's victim (value the mirror already tracks).
3. Rank resolution from `spell_dbc` at startup off §3's rank-1 IDs.
4. Echo per-creature CDs = Command CD (guards dc double-dips).
5. Mirror AI: no changes needed; existing Seduce guard already covers the
   manual-Seduce-active case.
6. `.legion command` (force press) + `dumpstats` last-press breakdown (per-unit action,
   damage, threat delta).

### 5.8 Edge rulings (settled)

No target → press fails for the four types that need one; VW works. Anchor dead/CC'd →
fails. Greater demon only (no legionnaires) → still valid. Target dies mid-imp-volley →
remaining bolts fizzle, no retarget. Manual Seduction active → Presence skips the
charmed unit; echoes follow the anchor's attack target (never the seduced unit).

---

## 6. Doombrand — capstone spec (granted by gwd)

### 6.1 Spell

| Field | Value |
|---|---|
| Spell | `290014` (talent-granted; costs no budget slot) |
| Cost | 1 Soul Shard · instant · 30yd · on GCD · **CD 20s** · debuff `290504` **10s** |
| Concurrency | One brand at a time (CD math enforces it). Not dispellable (no dispel type). |

**Loop:** brand → Command Demon → Empowerment window → army charges the sigil →
detonation → ~10s downtime → re-brand. Lining the army's best 10s inside the brand is
the skill expression; dc's CD reduction and Wild Imp timing are the supporting decisions.

### 6.2 Charging

- Stores `StorePct` (15%, conf) of damage dealt **by warlock-owned demons** to the
  branded target. Warlock's own damage (incl. fcd proc bolts) does NOT charge.
- Cap: `CapSPCoef × owner SP` (start 6.0, conf).
- **Charge gauge:** set debuff stack count = stored/cap in 5% steps (0–20). Exact value
  in `.legion dumpstats`.
- No recursion: detonation is warlock damage → can't feed Soul Harvest (demon-only
  proc) or a brand (demon-only accumulator). Both fall out of existing rules — verify
  in testing, don't build anything.

### 6.3 Detonation (`290505`)

| Trigger | Behavior |
|---|---|
| Natural expiry | Stored amount hits the branded target. Shadowflame, attributed to warlock, fired from the sigil (no caster LoS/range requirement). |
| Target dies branded | Immediate AoE at corpse: stored amount split evenly among enemies within 8yd (single enemy = full). |
| Evade/reset/despawn | Brand purged, NO detonation (anti-exploit). |

Detonation ignores demon damage multipliers (they already shaped the stored amount).

### 6.4 Tuning rails

Well-played window (3 legionnaires + Empowerment + Command press inside it) ≈ 1.5–2× a
Shadow Bolt. If average detonations sit at cap, the cap has become the balance and
`StorePct` is dead — raise cap or lower store.

### 6.5 Implementation hooks

1. Accumulator inside the existing demon damage hook: victim has `290504` from this
   owner → `stored += dmg × StorePct` (clamped), refresh stack gauge. Storage: struct
   keyed by target GUID in the pool manager (per-caster).
2. AuraScript `290504` OnRemove: `AURA_REMOVE_BY_EXPIRE` → single-target `290505`;
   `AURA_REMOVE_BY_DEATH` → AoE split variant at target position; evade/despawn → purge.
3. SpellScript `290014`: shard spend, apply aura, init accumulator at 0.
4. `290505`: triggered, damage set dynamically (`SetHitDamage`), caster = warlock.
5. `.legion brand` (force apply/detonate) + `dumpstats` brand line (target, stored, cap,
   time left).

### 6.6 Future talent hooks (not in scope)

Chance not to consume brand; expiry-splash; detonation heals demons (pairs with bt);
"your Shadow Bolts charge the sigil at half rate" (the caster-lean knob, if ever wanted).

---

## 7. Shard economy — Path B (normative)

| Active | Cost |
|---|---|
| Command Demon | 1 |
| Doombrand | 1 |
| Summon Wild Imps | 2 (il reduces to 1) |
| Legion Standard (if built, §12.1) | 2 |
| Legionnaire resummon | 1 |

- **No shard cap** (cut 2026-08-18 — a hard cap fights vanilla WoW flavor; shards are
  normal bag items and stay uncapped). Actives just spend them; hoarding is fine.
- Generation target: ~1 shard / 8–12s sustained with 2–3 demons (existing ICD is the
  lever). cm accelerates via crits.
- **Feel metric:** "wanted to press Command Demon / Doombrand but couldn't afford it"
  ≈ never in sustained fights. If frequent: raise generation before lowering costs.
- **Path A fallback** (if the throttle feels like friction after real play): cut sh/cm,
  keep il's cast-time half, backfill freed nodes with per-type **signature talents**
  (Presence ranks; Spell Lock extension; Cleave +1 target; echo damage %; "manual
  Seduction no longer breaks from your own dots").

---

## 8. Shared subsystems to build once

### 8.1 `OnPoolChanged` owner-aura driver
Currently a stub. Fires on recruit/dismiss/evict/death/login-restore with the new pool
composition. Consumers: **op** (per-demon owner HP/haste aura), **gwd Legion Aura**
(party +5% dmg/haste while pool ≥1), **cv owner half** (flat owner stamina — doesn't
strictly need pool state, but implement in the same owner-aura apply/remove pass).
Implementation: recompute a single owner aura's amounts from (talent ranks × pool
count); party aura as a standard 40yd area aura toggled by pool ≥1.

### 8.2 Demon-death event hook
One hook on owned-demon death → consumers: **dr** (chance instant resummon, 60s ICD),
**bbb** (survivors gain damage/haste; owner refunds 1 shard). Fire before corpse
cleanup; must not fire on dismiss/stash/logout (real deaths only).

### 8.3 Signature autocast wiring (feel work, no new spells)
Legionnaires + anchor autocast their vanilla kit: VW Torment (mirror-target), succubus
ambient Lash, felhound passive resist stance, felguard ambient Cleave when ≥2 targets.
**Reserved (never autocast): Seduction, Spell Lock, Intercept** — deliberate/manual or
Command-press only. No mana model; per-ability CDs. Imp Firebolt already exists as the
model.

---

## 9. Build order (phases with acceptance criteria)

**Phase 0 — housekeeping (before any feature):**
`git init` the module (first commit = current state). Build the live `.conf` (kill the
"Missing property" warnings); migrate existing tunables (Doom Bolt/Blast, multipliers)
into it. *Accept:* server reads conf; a value change applies without rebuild.

**Phase 1 — cheap shared hooks + easy talents:**
Demon-death hook (§8.2) → dr + bbb. `OnPoolChanged` (§8.1) → op + cv owner half +
Legion Aura groundwork. pf in the damage hook. *Accept:* `.legion dumpstats` shows dr
ICD state, op stacks matching pool count; killing a legionnaire procs dr/bbb; pf crits
appear flagged in logs at talent rate ±1%.

**Phase 2 — Path B economy:**
Shard costs on `290001` (imps) + resummons; generation retune; il rework; cm.
*Accept:* costs deducted; feel-metric session logged (§7).

**Phase 3 — Command Demon (§5):**
`290013`, dispatcher, five signatures (Enthralling Presence incl. imperative threat
drop), five echoes, two greater-demon responses, edge rulings, `.legion command`.
dc wired. *Accept:* every anchor-type press verified vs §5.8 table; Presence skips an
owner-charmed unit in test; threat delta visible in dumpstats.

**Phase 4 — Empowerment spine as one package:**
uv, cotp, re, se, **sl (absorb redesign)** — design the stacking math together, and
model it **with a fully-charged Doombrand window included** (the brand converts every
spine multiplier into stored burst). *Accept:* spreadsheet (or conf-comment) of the
worst-case multiplier stack; all five in conf.

**Phase 5 — Doombrand (§6):**
Accumulator, aura script, detonation, gauge, tooling. gwd rework (grant `290014` +
Legion Aura rider on the Phase-1 `OnPoolChanged` work; delete free-summon effect).
*Accept:* stack gauge tracks charge; death-AoE splits; evade purges without detonating;
detonation ≈ 1.5–2× Shadow Bolt in the reference window.

**Phase 6 — remaining ⬜ batches:**
Summon economy (rc + il cast-time), wl + bt, iwi + wotl (Firebolt script), fcd
(remember: its bolts don't charge Doombrand), vc (Health Funnel script), fb, gb, bor,
finish si caster half. *Accept:* per-talent smoke test via dumpstats.

**Phase 7 — feel pass:**
Signature autocast wiring (§8.3). Then the §12 decisions (Legion Standard / rw) with
the banked spell slot.

---

## 10. Balance guardrails

- **Snowball audit:** fa × vp × cotp × gb × Empowerment (+ sl absorbs, + Doombrand
  conversion) is the compounding risk. Phase-4 model is mandatory before tuning up.
- **Optional taper (conf, default off):** each legionnaire beyond the first contributes
  90%/80% of full damage — keeps 3-slot builds strong without linear tripling. Implement
  as a multiplier in the damage hook keyed by pool index.
- Doombrand `StorePct` starts low; buff upward.
- Detonation-at-cap check (§6.4) is a standing tuning alarm.

---

## 11. Known limitations & standing items

- Greater demons don't persist across logout; with es making them "permanent," decide
  deliberately whether to add death/mount stash + logout persistence (recommended:
  yes, match legionnaires) or document as intended.
- Respec-away with a greater demon out doesn't despawn it until logout.
- Pet-mod dual-spec re-sync edge cases untested.
- Damage hook applies ×1.0 globally when talents absent — harmless; scope to module
  entries only if it ever bites.
- Doom Bolt 850 + 0.64×SP; Doom Blast 300 + 0.50×SP (in conf as of Phase 0).

## 12. Open decisions (do not block Phases 0–6)

1. **Legion Standard** — the banked baseline-spell slot's standing candidate: planted
   banner = stationary creature entry + area aura on existing guardian plumbing; costs
   2 shards; unblocks **fs** as written. Decide after Command Demon + Doombrand have
   been played (the kit may already feel complete, in which case bank the slot
   permanently and redesign fs, e.g. onto the Legion Aura rider).
2. **Riftwalker (rw) redesign approval** — proposed: Demonic Circle: Teleport also
   warps commanded demons to you + demons +30% move speed 6s. Zero new spells (rides
   baseline Demonic Circle). Approve, tweak, or propose alternative.
3. **Taper on/off default** (§10) — decide after first real tuning pass.
4. **Command Demon off-GCD?** — shipping on-GCD; revisit after Doombrand windows are
   playable (off-GCD makes the opener burstier).
