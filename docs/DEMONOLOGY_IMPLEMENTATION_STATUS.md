# Demonology Rework — Talent & Spell Implementation Status

**Module:** `mod-demonology-rework` (AzerothCore 3.3.5a)
**As of:** 2026-08-18
**Purpose:** A single reference of what is / isn't implemented in the Demonology rework —
talents, spells, creatures, and the systems behind them — for design discussion before
building out the remaining tree.

Legend: ✅ implemented & in-game · 🟡 partial · ⬜ not implemented (buildable on current
systems) · 🔒 blocked (needs an ability/system that doesn't exist yet).

---

## 1. System backbone (what actually exists)

These are the engine-level systems the talents plug into. All are live and verified in-game.

| System | Status | Notes |
|---|---|---|
| **Command Pool** | ✅ | One pool/warlock. Anchor pet (slot 0) + up to N legionnaires commanded as one unit. |
| **Slot cap from talents** | ✅ | Base 0; Expanded Command / II / Legion Commander each +1 (max 3). |
| **Legionnaire roster (per-type summons)** | ✅ | Imp/Felguard/Succubus/Felhound/Voidwalker, mixed comp, evict-oldest at cap. |
| **Mirroring AI** | ✅ | Legionnaires inherit the anchor's react-state + target; follow/formation; CC-aware (never break the anchor's Seduce or the owner's Fear/Poly). |
| **Unified defensive response** | ✅ | Any attack on the owner / pet / any legionnaire / greater demon rallies **every** demon (mirror + auto-assist AI share `FindLegionThreat`). |
| **Stat inheritance (PetScaling)** | ✅ | Demon health = %owner HP; melee = SP-derived; live re-apply on gear/talent/SP change. Applies to legionnaires, wild imps, greater demons **and** the anchor pet. |
| **Demon damage scaling hook** | ✅ | Melee + spell damage of any warlock-owned demon scaled by talent multipliers at hit time. |
| **Shard economy (Soul Harvest)** | ✅ | Per-player ICD shard generation on demon damage; loot-tagging fixed for owned-creature damage. |
| **Hybrid-learn** | ✅ | Talents/known-pets grant the matching summon spells into the Demonology spellbook tab; removed on respec. |
| **Greater demons (Infernal/Doomguard)** | ✅ | Cooldown guardians; SP-scaling Doom Bolt/Blast; cost command slots (Infernal 2 / Doomguard 3) so they can't stack on a full legion. |
| **Persistence** | ✅ | Legionnaires survive logout/login (incl. health%); stash on death/mount. **Greater demons do NOT persist across logout.** |
| **Demonic Empowerment** | ✅ | Buffs pet + legionnaires + wild imps + the active greater demon. |
| **`.legion` GM tooling** | ✅ | `pool`, `dumpstats` (talent ranks, damage mults, per-unit hit/swing, greater-demon line), `recruit/dismiss/summon/shards`. |
| **Client talent tree render** | ✅ | 36-node tree renders via the `DemoTalentFix` addon (works around a client FrameXML draw crash). |

---

## 2. Spells & creatures (implemented content)

**Player-cast spells** (custom IDs; all in-game):
- `290000` Demonic Empowerment · `290001` Summon Wild Imps
- `290002–290006` Summon **Legionnaire** Imp/Felguard/Succubus/Felhound/Voidwalker
- `290007` Summon Infernal · `290008` Summon Doomguard
- `290010–290012` Soul Harvest (ranks 1–3, granted by the Soul Harvest talent)

**Pet/guardian ability spells:** `290900` Wild Imp Firebolt · `290901` Doom Bolt · `40878` Doom Blast (AOE, damage capped by us).

**Internal:** `290500` Demonic Empowerment buff.

**Creatures:** Wild Imp (`600000`); base pet entries reused as legionnaires (Imp 416, Felhunter 417, Voidwalker 1860, Succubus 1863, Felguard 17252); greater demons Infernal 89, Doomguard 11859.

**Gating of base pets:** Felguard pet (`30146`) is gated behind the Summon Felguard talent; the four basic pets (Imp/VW/Succ/Felhunter) are learned normally and their **legionnaire** version unlocks once known **and** Expanded Command is trained.

---

## 3. Talent tree — full status (36 nodes)

Tree = 11 tiers, layout `[4,3,4,3,4,3,4,4,3,3,1]`, 78 points, tab 303 ("Demonology"/Master of the Legion). Marker = the rank-1 spell C++ reads via `HasTalent`.

| Tier | Talent | key | Ranks | Marker | Status | Effect / why blocked |
|---|---|---|---|---|---|---|
| T1 | Fel Conditioning | fc | 5/10/15% | 290904 | ✅ | Demon health %. |
| T1 | Improved Legion | il | — | 290902 | ⬜ | Legionnaire summons cast 0.5s faster + cost 1 less shard. Needs cast-time/cost mod. |
| T1 | Soul Harvest | sh | (290010-12) | — | ✅ | Shard generation on demon damage. |
| T1 | Cursed Vitality | cv | 3/6% | 290907 | 🟡 | Demon +stamina→health **done**; **owner +3/6% stamina TODO** (player stat aura). |
| T2 | Rapid Conjuration | rc | 1/2/3 | 290909 | ⬜ | Summon cast time −1.5/3/4.5s; rank 3 castable while moving. |
| T2 | Cruel Master | cm | 1/2 | 290915 | ⬜ | Demon crits double Soul Harvest proc chance + reduce its CD. Extends the shard economy. |
| T2 | Fel Armory | fa | 5/10/15% | 290912 | ✅ | +demon damage while Fel Armor is up. |
| T3 | Expanded Command | ec | +1 | 290922 | ✅ | +1 legionnaire slot. |
| T3 | Vital Conduit | vc | 20/40% | 290923 | 🔒 | Health Funnel efficiency + no self-damage while channeling. Needs Health Funnel interaction. |
| T3 | Pactbound Fury | pf | 2/4/6% | 290917 | ⬜ | Demon crit chance. Needs a creature-crit mechanism (no clean setter). |
| T3 | Fel Blood | fb | 15/30% | 290920 | 🔒 | Fel Lash damage + killing blows refresh Corruption. **Fel Lash ability doesn't exist.** |
| T4 | Improved Wild Imps | iwi | 5/10 | 290930 | ⬜ | +Wild Imp duration; Firebolt 10/20% chance to hit a 2nd target. |
| T4 | Shadowflame Legion | sl | 15/30% | 290928 | 🔒 | Hand of Gul'dan grants demons a shield. **Hand of Gul'dan doesn't exist.** |
| T4 | Savage Instincts | si | 4/8/12% | 290925 | 🟡 | Demon **melee** attack speed **done**; **caster cast-speed TODO**. |
| T5 | Expanded Command II | ec2 | +1 | 290932 | ✅ | +1 legionnaire slot. |
| T5 | Blood Tithe | bt | 4/8% | 290936 | ⬜ | Demons heal you for % of damage dealt (doubled at 3+ demons). |
| T5 | Warded Legion | wl | 9/18% | 290938 | ⬜ | Demon chance to fully resist spells; rank 2 Fear/Charm/Poly immunity. Proc/aura. |
| T5 | Vicious Pact | vp | 8/16/24% | 290933 | ✅ | SP→demon melee (AP) + SP→demon spell power. |
| T6 | Demonic Rebirth | dr | 50/100% | 290940 | ⬜ | On demon death, chance to instantly resummon it (60s CD). Proc on demon death. |
| T6 | Dark Command | dc | 3/6/10 | 290942 | 🔒 | Command Demon CD + haste. **Command Demon system doesn't exist.** |
| T6 | Unholy Vigor | uv | 1/2/3 | 290945 | ⬜ | Demonic Empowerment duration +1/2/3s. Extends existing buff. |
| T7 | Summon Felguard | sfg | 1 | 290948 | ✅ | Gates the Felguard pet + legionnaire (hybrid-learn). |
| T7 | Wrath of the Legion | wotl | 10/20/30% | 290949 | ⬜ | Wild Imp Firebolt chance to summon an extra imp. Proc on Firebolt. |
| T7 | Grim Bargain | gb | 6/12 | 290952 | 🔒 | Grimoire of Synergy duration/damage. **Grimoire system doesn't exist.** |
| T7 | Fervent Standard | fs | 5/10 | 290954 | 🔒 | Legion Standard radius/health/damage. **Legion Standard doesn't exist.** |
| T8 | Overlord's Presence | op | 2/4/6% | 290956 | ⬜ | Per commanded demon: +owner max health & haste. Owner aura scaling with demon count. |
| T8 | Fel Conduit | fcd | 5/10% | 290959 | ⬜ | Demon attacks proc an instant/free Shadow Bolt (stacking). Proc → player Shadow Bolt. |
| T8 | Riftwalker | rw | 1 | 290961 | 🔒 | Rend Veil CD/duration. **Rend Veil doesn't exist.** |
| T8 | Cruelty of the Pit | cotp | 5/10/15% | 290962 | ⬜ | Empowered demons deal +damage. Extends Demonic Empowerment. |
| T9 | Bound by Blood | bbb | 15/30% | 290974 | ⬜ | On demon death, the rest gain damage/haste + you regain a shard. Proc on demon death. |
| T9 | Beacon of Ruin | bor | 30/60% | 290969 | 🔒 | Metamorphosis duration + damage reduction. **Metamorphosis doesn't exist.** |
| T9 | Ruinous Empowerment | re | 7/14/20% | 290966 | ⬜ | Demonic Empowerment grants leech + no-expire chance. Extends the buff. |
| T10 | Legion Commander | lc | +1 | 290971 | ✅ | +1 legionnaire slot (3rd). |
| T10 | Eternal Servitude | es | 1 | 290965 | ✅ | Infernal/Doomguard permanent + 60s CD (requires Legion Commander). |
| T10 | Supreme Empowerment | se | 3/6 | 290972 | ⬜ | Demonic Empowerment affects temp demons + lasts longer. (Temp-demon part largely done; needs gating/duration.) |
| T11 | Grand Warlock's Design | gwd | 1 | 290976 | 🔒 | Legion Aura (party +5% dmg/haste) + free/instant first summon in combat. **Legion Aura / party-buff system doesn't exist** (`OnPoolChanged` hook is a stub). |

### Summary
- **✅ Implemented (9):** fc, sh, fa, ec, ec2, vp, sfg, lc, es
- **🟡 Partial (2):** cv (owner-stamina half), si (caster cast-speed half)
- **⬜ Buildable on current systems (17):** il, rc, cm, pf, vc*, iwi, bt, wl, dr, uv, wotl, op, fcd, cotp, bbb, re, se
- **🔒 Blocked on unbuilt systems (8):** fb, sl, dc, gb, fs, rw, bor, gwd

`*vc` marked 🔒 in the table because it hooks Health Funnel; listed here as borderline-buildable.

---

## 4. What the ⬜ talents need (buildable now)

Grouped by the mechanic they'd share, so they can be planned in batches:

- **Proc on demon death:** Demonic Rebirth (dr), Bound by Blood (bbb) — hook a demon-death event.
- **Proc on Firebolt / Wild Imps:** Improved Wild Imps (iwi), Wrath of the Legion (wotl) — extend the Firebolt script + wild-imp summon.
- **Extend Demonic Empowerment:** Unholy Vigor (uv, duration), Cruelty of the Pit (cotp, +dmg), Ruinous Empowerment (re, leech), Supreme Empowerment (se, temp demons + duration) — all layer onto the existing DE buff.
- **Extend Soul Harvest:** Cruel Master (cm) — crit-driven proc + CD.
- **Demon defensive/utility:** Warded Legion (wl, resist + CC immunity), Blood Tithe (bt, leech to owner).
- **Owner-side auras:** Overlord's Presence (op), Cursed Vitality owner half (cv), Fel Conduit (fcd, procs the player's Shadow Bolt).
- **Summon economy:** Improved Legion (il), Rapid Conjuration (rc) — cast-time + shard-cost mods.
- **Creature crit:** Pactbound Fury (pf) — needs a demon-crit approach (the only tricky one in this bucket).

---

## 5. Unbuilt abilities/systems the 🔒 talents depend on

These talents can't be wired until the underlying ability exists. Each is a design decision in
its own right:

| System | Talents waiting on it |
|---|---|
| **Metamorphosis** (demon-form) | Beacon of Ruin (bor) |
| **Hand of Gul'dan** | Shadowflame Legion (sl) |
| **Fel Lash** | Fel Blood (fb) |
| **Command Demon** (per-type demon command ability) | Dark Command (dc) |
| **Grimoire of Synergy** (duplicate demon) | Grim Bargain (gb) |
| **Legion Standard** (planted banner) | Fervent Standard (fs) |
| **Rend Veil** (rift) | Riftwalker (rw) |
| **Legion Aura / party-buff spine** | Grand Warlock's Design (gwd) |
| **Health Funnel interaction** | Vital Conduit (vc) |

Also referenced by talents but not built: **per-type demon signature abilities** (Succubus
Seduce, Felhound Spell Lock/interrupt, Voidwalker taunt/shield, Infernal immolation aura,
Felguard cleave/Felstorm). The mirror is already *CC-aware* of a Succubus Seduce, but no demon
actually casts a signature ability yet — the imp's Firebolt is the only one.

---

## 6. Known limitations & polish items

- **Greater demons don't persist across logout** (despawn on logout; no death/mount stash). Respec-away while one is out doesn't despawn it until logout.
- **Savage Instincts** hastes demon **melee** only; caster demons' (imp/Doomguard) cast-speed portion not yet applied.
- **Cursed Vitality** owner-stamina half not applied (demon-health half is).
- **Pet-mod dual-spec:** talent-driven pet buffs re-sync on talent change/login; edge cases around dual-spec swaps are untested.
- **Demon damage hook** applies the vp/fa multiplier to *any* warlock-owned demon (×1.0 when the owner lacks the talents) — harmless but global; could be scoped to module entries if desired.
- **Doomguard tuning:** Doom Bolt (single-target) 850 + 0.50×SP; Doom Blast (AOE) 300 + 0.50×SP. All damage values are config-tunable.
- **No live `.conf`** yet — the server runs on code defaults (benign "Missing property" warnings). A live conf would allow rebuild-free tuning.
- **Module is not a git repo** yet.

---

## 7. Design questions worth resolving before building more

1. **Signature abilities vs. more passives:** the tree has many passive multipliers; the biggest
   *feel* gap is per-type demon signatures (what makes picking a Succubus vs Felhound matter).
2. **The Empowerment spine** (uv/cotp/re/se + gwd) is a cluster of buffs layered on Demonic
   Empowerment — worth designing as one coherent package.
3. **The "on demon death" cluster** (dr/bbb) and **proc clusters** — decide the event hooks once.
4. **Which 🔒 abilities are in scope at all** (Metamorphosis, Rend Veil, Grimoire, Legion Standard,
   Command Demon, Hand of Gul'dan, Fel Lash) — several are large features; some talents may be
   re-designed to fit what's built rather than building all of them.
