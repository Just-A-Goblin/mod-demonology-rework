# Phase 6 Test Plan — talent batch (bt, bor, iwi, wotl, wl, gb, fcd, fb)

Covers the eight talent nodes built in Phase 6. Parked/deferred nodes (rc, si caster half,
vc, rw, fs) are **out of scope** here.

## Setup / harness

- **Character:** level 80 warlock, GM (Legiontesttw). `.die`-proof; use `.cheat god` if handy.
- **Talent points:** `.reset talents` between builds to reallocate. Deep nodes need points:
  fb T3 (10 pts), iwi T4 (15), wl/bt T5 (20), gb/wotl T7 (30), fcd T8 (35), bor T9 (40).
- **Primary oracle:** `.legion dumpstats` — every node prints its live values (chance, %, ranks).
  Read it first after each spec to confirm the talent registered.
- **Demons:** anchor pet (`.cast 688/697/…` or summon normally) + legionnaires
  (`.legion recruit 416|417|1860|1863|17252`), Wild Imps (Summon Wild Imps, needs shards:
  `.additem 6265 20`), greater demons (Summon Infernal/Doomguard — needs Eternal Servitude —
  or `.legion summon 89` / `.legion summon 11859`).
- **Targets:** a dummy or a pack of low-level mobs; for AoE/refresh tests you need 2–4 clustered.
- **Tuning:** all numbers are in `mod_demonology_rework.conf`; edit + `.reload config` to retune
  without a restart.

Run a quick **regression smoke test** first: summon demons, Command Demon, Doombrand a target,
Demonic Empowerment — confirm nothing Phase 0–5 broke.

---

## 1. Blood Tithe (bt) — T5, prereq vc@2
**Spec:** vc 2/2 (prereq only, no effect), bt 1→2.
**Steps:**
1. `.legion dumpstats` → "Blood Tithe: heal X% of demon damage (N demons -> base/DOUBLED)".
2. Drop to partial HP. Send 1–2 demons at a mob; watch your HP tick **up** as they deal damage.
3. Now command **3+** demons and repeat.
**Expected:** heal = 4% (r1) / 8% (r2) of each demon hit; **doubles** (8/16%) at ≥3 commanded
demons. No heal from *your* damage — demons only. No heal while you're dead.

## 2. Beacon of Ruin (bor) — T9
**Spec:** climb to T9, bor 1→2. (Eternal Servitude to get the permanent greater-demon summons.)
**Steps:**
1. `.legion dumpstats` → "Beacon of Ruin: rank X -> greater demons +Y% damage, summon CD -Z%".
2. **Damage:** summon Infernal/Doomguard; note its hit / Doom Bolt numbers vs a no-bor baseline.
3. **Cooldown:** cast Summon Infernal, open the spellbook/CD — cooldown should be base −20/40%.
**Expected:** only Infernal/Doomguard damage is boosted (not pet/legionnaires/imps). CD visibly
shorter at rank 2.

## 3. Improved Wild Imps (iwi) — T4
**Spec:** iwi 1→2.
**Steps:**
1. `.legion dumpstats` → "+Ns duration, X% Firebolt 2nd-target".
2. Summon Wild Imps; time how long they persist (base 20s + 5/10s).
3. Stand imps near a **pack** (2+ mobs); watch the combat log — a Firebolt should occasionally
   land a second hit on a nearby mob (10/20%).
**Expected:** longer imp lifespan; ~10/20% of Firebolts produce a 2nd-target bolt within 8yd.

## 4. Wrath of the Legion (wotl) — T7, prereq iwi@2
**Spec:** iwi 2/2, wotl 1→3.
**Steps:**
1. `.legion dumpstats` → "Wrath of the Legion: X% spawn (max 2/cast)".
2. Summon Wild Imps into sustained combat; watch the imp count climb past the base 3 as Firebolts
   spawn extra imps.
**Expected:** ~10/20/30% of Firebolts spawn one extra imp. **Regression guard:** the count must
**not** run away — the per-imp chain budget caps chains at 2/cast. Confirm it plateaus, not explodes.

## 5. Warded Legion (wl) — T5, prereq fc@2
**Spec:** fc 2/3 (prereq), wl 1/2 then 2/2.
**Steps:**
1. `.legion dumpstats` → "Warded Legion: X% spell resist [+ Fear/Charm/Poly immune at r2]".
2. **Resist:** have a caster mob attack a demon; watch the demon's incoming spells occasionally
   fully miss/resist (~9/18%). (Statistical — sample several casts, or trust dumpstats + a few
   observed resists.)
3. **CC immunity (rank 2):** hit a demon with a Fear/Polymorph (a fear-casting mob, or
   `.cast 5782` a Fear on the demon). At rank 2 it should show **Immune**; at rank 1 it should be
   feared.
**Expected:** resist chance scales 9→18%; CC immunity ONLY at rank 2 (rank 1 = no immunity).

## 6. Grim Bargain (gb) — T7
**Spec:** gb 1→2.
**Steps:**
1. `.legion dumpstats` → "Grim Bargain: 15% chance -> +X% dmg".
2. **Demon→owner:** fight with demons; watch for a **Grim Bargain** buff to appear on **you**
   (+6/12% damage, 8s).
3. **Owner→demon:** deal damage yourself (Shadow Bolt); watch the **Grim Bargain** buff appear on
   your **demons**.
**Expected:** ~15% proc either direction; buff is +6% (r1) / +12% (r2) damage for 8s. Both
directions fire independently.

## 7. Fel Conduit (fcd) — T8  ← most novel, verify carefully
**Spec:** fcd 1→2.
**Steps:**
1. `.legion dumpstats` → "Fel Conduit: X% proc".
2. Fight with demons attacking; watch for a **Fel Conduit** buff (up to 3 charges) on you.
3. With the buff up, cast **Shadow Bolt**:
   - cast bar should be **instant** (no cast time),
   - **no mana** consumed,
   - one Fel Conduit charge consumed per cast (3 → 0),
   - Shadow Bolt **damage unchanged**.
**Expected:** 5/10% proc per demon attack; instant+free SB while charged. **Regression guard:**
these proc-bolts are *your* damage — confirm they do **not** charge a Doombrand sigil or generate
Soul Shards (Soul Harvest).

## 8. Fel Blood (fb) — T3
**Spec:** fb 1→2.
**Steps:**
1. `.legion dumpstats` → "Fel Blood: Lash +X%".
2. **Lash damage:** with a Succubus anchor, press Command Demon (fires the Empowered Lash echo
   290502) or use Lash of Pain; compare damage vs no-fb baseline (+15/30%).
3. **Corruption refresh:** cast your Corruption on 3–4 clustered mobs; let a demon land the
   **killing blow** on one. The Corruption on the nearby survivors (within 10yd) should jump back
   to **full duration**.
**Expected:** Lash hits 15/30% harder; a demon kill refreshes your Corruption on nearby enemies.

## 9. Riftwalker (rw) — T8  (added after §12.2 approval)
**Spec:** rw 1/1. Character needs Demonic Circle (`.learn 48018` / `.learn 48020`).
**Steps:**
1. `.legion dumpstats` → "Riftwalker: TRAINED (... warps demons + 30% speed 6s)".
2. Cast **Demonic Circle: Summon** (48018) to plant a circle; run away from it with demons out.
3. Cast **Demonic Circle: Teleport** (48020).
**Expected:** you + all commanded demons (pet + legionnaires + greater demon) snap to the circle;
demons gain a Riftwalker +30% move-speed buff for 6s. Untrained = only you teleport (demons stay).

---

## Sign-off checklist
- [ ] dumpstats shows correct values for all 8 nodes at rank 1 and max rank
- [ ] bt heals; doubles at 3+ demons
- [ ] bor boosts only greater demons; CD reduced
- [ ] iwi longer imps + 2nd-target bolts
- [ ] wotl spawns extra imps AND is bounded (no runaway)
- [ ] wl resist observed; CC immunity gated to rank 2
- [ ] gb procs both directions
- [ ] fcd: instant + free Shadow Bolt, charges consume, damage unchanged, no Doombrand/Soul Harvest feed
- [ ] fb Lash bonus + Corruption refresh
- [ ] rw warps demons to the circle + speed buff
- [ ] regression: Doombrand / Command Demon / Empowerment / summons unaffected
