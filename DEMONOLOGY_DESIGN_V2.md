# Demonology Rework — Design & Implementation Doc (v2)

**Module:** `mod-demonology-rework` (AzerothCore 3.3.5a, solo server + playerbots)
**As of:** 2026-08-20
**Supersedes:** DEMONOLOGY_IMPLEMENTATION_STATUS.md (v1)
**Audience:** implementation by Claude Code.

> **STATUS: COMPLETE & LIVE (2026-08-20).** All 36 talent nodes, both baseline abilities, the
> Command Demon and Doombrand capstones, and every subsystem are built, deployed, and playtested.
> A balance pass (FLATTEN) has been applied. This doc has been synced to the shipped module — the
> `⬜` "not implemented" markers below in §2.2 / §3 are historical (everything there is now built).
> The always-current node audit is `DEMONOLOGY_STATUS.md`; balance is `docs/balance_model.py` +
> the balance-model memory. Remaining work is validation only (real 60/80 combat parses).

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
| Hybrid-learn | Talent/known-pet → summon spells in Demonology tab; removed on respec. (Applies to the LEGIONNAIRE summons + Command Demon + Doombrand only — those are talent-granted.) |
| Greater demons | Infernal (2 slots) / Doomguard (3 slots); SP-scaled Doom Bolt/Blast. |
| Persistence | Legionnaires survive logout (health%); stash on death/mount. Greater demons do NOT persist (see §11). |
| Demonic Empowerment | Buffs pet + legionnaires + wild imps + active greater demon. **TRAINER-taught @ level 50** (warlock trainer 31/32, base SQL `33_baseline_trainer_spells.sql`; matches live WoW's mid-50s). |
| Summon Wild Imps | Path B core active. **TRAINER-taught @ level 10** (warlock trainer 31/32). Rank-less single ability. Learn = trainers or talents ONLY — no hybrid/level-auto for these two (user directive). |
| `.legion` GM tooling | pool/dumpstats/recruit/dismiss/summon/shards. Extensions in §5.7, §6.6. |
| Client tree render | 36 nodes via `DemoTalentFix` addon. |

### 2.2 New systems (all ✅ BUILT & live — this section was the original spec)

| System | Spec | Consumed by |
|---|---|---|
| **Command Demon** (dispatch: anchor signature + per-type echoes + greater-demon responses) | §5 | dc talent; signature identity |
| **Doombrand** (brand → army charges sigil → detonation) | §6 | gwd capstone |
| **Path B shard costs** (actives spend shards) | §7 | il, cm, bbb re-reads |
| **`OnPoolChanged` owner-aura driver** | §8.1 | op, cv owner half |
| **Demon-death event hook** | §8.2 | dr, bbb |
| **Signature autocast wiring** | §8.3 | per-type demon feel; future signature talents |
| **Enthralling Presence** (succubus signature) | §5.5 | Command Demon |

---

## 3. Spell & creature ID registry

> All IDs below are BUILT and in `data/spellforge/ids.yaml` (the live allocation). The `⬜` marks
> here are historical. Redesigns added: `290516` Vital Conduit heal, `290517` Fervent Standard icon.

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

## 4. Talent tree — 36 nodes (ALL ✅ implemented & live)

Tab 303, 78 points, marker = rank-1 spell read via `HasTalent`. **The tree was RESHUFFLED
2026-08-20** — current per-tier layout `[4,3,4,3,4,3,4,4,2,4,1]` (node counts) after moving the
Soul Harvest cluster to T3, Fel Armory to T1, and Beacon of Ruin to T10. Effects/columns below are
the SHIPPED versions; `docs/balance_model.py` carries the balance numbers (FLATTEN pass applied).

| Tier | Talent | key | Ranks | Marker | Status | Effect (shipped) |
|---|---|---|---|---|---|---|
| T1 | Fel Conditioning | fc | 5/10/15% | 290904 | ✅ | Demon health %. |
| T1 | Improved Legion | il | -15/-30% | 290902 | ✅ | **Mana-efficiency node** (reworked): reduces Summon Wild Imps' mana cost via a real SPELLMOD_COST (needs its own family bit so Fel Conduit can't steal it). No longer cast-time/shard. |
| T1 | Fel Armory | fa | 5/10/15% | 290912 | ✅ | +demon damage while Demon Skin / Demon Armor / **Fel Armor** is up (first-in-chain check). Melee half is real Attack Power on the anchor pet (shows in the pet tab). |
| T1 | Cursed Vitality | cv | 3/6% | 290907 | ✅ | Demon stamina→health **and** owner +3/6% stamina (OwnerMods.cpp). Both halves done. |
| T2 | Vital Conduit | vc | 50/100% | 290923 | ✅ | **REDESIGNED (Addendum A.1):** your Life Tap also heals your commanded demons for 50/100% of the health sacrificed (overheal allowed). Keeps the life-trade flavor. bt's prereq. |
| T2 | Pactbound Fury | pf | 2/4/6% | 290917 | ✅ | Real demon crit chance (melee outcome roll + spell crit), not an invisible multiply. |
| T2 | Fel Blood | fb | 15/30% | 290920 | ✅ | Succubus Lash of Pain (echo `290502` + signature `290515` + vanilla chain) +15/30%; demon killing blows refresh your Corruption nearby. |
| T3 | Expanded Command | ec | +1 | 290922 | ✅ | +1 legionnaire slot. **Fel Conditioning prereq removed** (user). |
| T3 | Cruel Master | cm | 1/2 | 290915 | ✅ | Demon crits accelerate Soul Harvest (2× proc / −ICD). **Requires Soul Harvest** (T3, left of it). |
| T3 | Soul Harvest | sh | R1–3 | 290010-12 | ✅ | Shard generation on demon damage (Path B). Moved to **T3**, flanked by cm/rc. Hidden from spellbook (do_not_display). |
| T3 | Fel Corruption | rc | 33/66/100% | 290909 | ✅ | **REDESIGNED (Addendum A.2, was Rapid Conjuration):** your Corruption's periodic damage counts as demon damage for Soul Harvest + Doombrand (½ weight) at rank effectiveness. **Requires Soul Harvest** (right of it). |
| T4 | Improved Wild Imps | iwi | 5/10 | 290930 | ✅ | +imp duration; Firebolt 10/20% chance to hit a 2nd target. |
| T4 | Shadowflame Legion | sl | 15/30% | 290928 | ✅ | Demonic Empowerment also shields affected demons for 15/30% of their max HP (Empowerment spine). |
| T4 | Savage Instincts | si | 4/8/12% | 290925 | ✅ | Demon melee attack speed **and** casting frequency (caster half scales the AI recast cadence + anchor pet cast-speed). |
| T5 | Expanded Command II | ec2 | +1 | 290932 | ✅ | +1 legionnaire slot. |
| T5 | Blood Tithe | bt | 4/8% | 290936 | ✅ | Demons heal you for % damage dealt (doubled at 3+ demons). Requires vc@2. |
| T5 | Warded Legion | wl | 9/18% | 290938 | ✅ | Demon spell-resist chance; R2 Fear/Charm/Poly immunity. |
| T5 | Vicious Pact | vp | 8/16/24% | 290933 | ✅ | SP→demon AP + SP→demon spell power. Melee half is real Attack Power on the anchor pet (pet tab). Requires si@3. |
| T6 | Demonic Rebirth | dr | 50/100% | 290940 | ✅ | On demon death, chance to instantly resummon (60s ICD). |
| T6 | Dark Command | dc | 3 | 290942 | ✅ | Command Demon CD −5/10/15s; responders get a short haste buff. |
| T6 | Unholy Vigor | uv | 1/2/3 | 290945 | ✅ | Demonic Empowerment +1/2/3s. |
| T7 | Summon Felguard | sfg | 1 | 290948 | ✅ | Gates Felguard pet + legionnaire. |
| T7 | Wrath of the Legion | wotl | 10/20/30% | 290949 | ✅ | Firebolt chance to spawn an extra Wild Imp (chain-capped); each bonus imp drains 5% base mana on spawn. |
| T7 | Grim Bargain | gb | 6/12% | 290952 | ✅ | Demon↔owner damage proc synergy (redesigned off the cut Grimoire duplicate). |
| T7 | Fervent Standard | fs | 4/8% | 290954 | ✅ | **REDESIGNED (Addendum A.3, was Legion Standard):** your Demonic Circle is the legion's banner — within 20yd, you+demons deal +4/8%, demons take −5/10%. Direct range checks vs the circle GameObject; cosmetic icon 290517 on owner+demons. |
| T8 | Overlord's Presence | op | 2/4/6% | 290956 | ✅ | Per commanded demon: +owner max HP & haste (`OnPoolChanged`). |
| T8 | Fel Conduit | fcd | 5/10% | 290959 | ✅ | Demon attacks proc an instant/free Shadow Bolt (3 charges). Proc bolts are player damage (don't charge Doombrand/Soul Harvest). |
| T8 | Riftwalker | rw | 1 | 290961 | ✅ | Demonic Circle: Teleport warps all commanded demons to you + 30% move speed 6s (redesigned off cut Rend Veil). |
| T8 | Cruelty of the Pit | cotp | 5/10/15% | 290962 | ✅ | Empowered demons +damage (Empowerment spine). |
| T9 | Bound by Blood | bbb | 15/30% | 290974 | ✅ | On demon death: survivors gain damage/haste; you regain a shard. |
| T9 | Ruinous Empowerment | re | 7/14/20% | 290966 | ✅ | Empowerment grants leech + no-expire chance (spine). |
| T10 | Legion Commander | lc | +1 | 290971 | ✅ | +1 (4th) legionnaire slot. |
| T10 | Eternal Servitude | es | 1 | 290965 | ✅ | Infernal/Doomguard permanent + 60s CD (req lc). |
| T10 | Beacon of Ruin | bor | 15/30% | 290969 | ✅ | Infernal/Doomguard +15/30% damage, −20/40% summon CD. **Moved to T10, requires Eternal Servitude** (no longer a stray inert node in a legionnaire build). |
| T10 | Supreme Empowerment | se | 3/6 | 290972 | ✅ | Empowerment affects temp demons + lasts longer. Requires re@3. |
| T11 | Grand Warlock's Design | gwd | 1 | 290976 | ✅ | Capstone: grants **Doombrand** (§6). |

### Summary
- **✅ All 36 nodes implemented & live.** The three former dead nodes (rc→Fel Corruption, vc,
  fs→Fervent Standard) were redesigned per Addendum A; il and si were finished; the tree was
  reshuffled and a FLATTEN balance pass applied. Nothing pending, nothing blocked.
- **Baseline abilities** (§2.1): Summon Wild Imps (trainer @10) and Demonic Empowerment (trainer @50).
- Balance: modeled Demo single-target ≈ +30% (60) / +45% (80) over the next spec; AoE weak by design.

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
Built (`OwnerMods`). Fires on recruit/dismiss/evict/death/login-restore with the new pool
composition. Consumers: **op** (per-demon owner HP/haste aura) and **cv owner half** (flat
owner stamina — doesn't strictly need pool state, but implemented in the same owner-aura
apply/remove pass). Implementation: recompute the owner mods' amounts from (talent ranks ×
pool count) and apply the delta only.

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
Demon-death hook (§8.2) → dr + bbb. `OnPoolChanged` (§8.1) → op + cv owner half.
pf in the damage hook. *Accept:* `.legion dumpstats` shows dr
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

**Phase 5 — Doombrand (§6): ✅ DONE**
Accumulator, aura script, detonation, `.legion brand` + dumpstats charge line, tooling.
gwd rework (grants `290014`; Legion Aura rider CUT as unnecessary; free-summon effect
gone). Charge shown in `.legion dumpstats` (no client stack gauge — SpellForge has no
stacks field). *Accepted in playtest:* death-AoE splits within 8yd; evade/dispel purges
without detonating; detonation reads as a real burst at `StorePct 0.50`.

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

1. **Legion Standard — RESOLVED (banked):** the kit feels complete; the banked baseline
   slot is NOT spent on a planted banner. Legion Standard is dropped and **fs is to be
   redesigned** into a smaller self-contained effect (TBD).
2. **Riftwalker (rw) — RESOLVED (approved as proposed):** Demonic Circle: Teleport also
   warps commanded demons to you + demons +30% move speed 6s. Building it.
3. **Taper on/off default** (§10) — decide after first real tuning pass.
4. **Command Demon off-GCD?** — shipping on-GCD; revisit after Doombrand windows are
   playable (off-GCD makes the opener burstier).
