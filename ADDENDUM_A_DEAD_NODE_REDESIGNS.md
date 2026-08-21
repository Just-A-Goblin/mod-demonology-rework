# Addendum A — Dead-Node Redesigns: Vital Conduit, Fel Corruption, Fervent Standard

**Module:** `mod-demonology-rework` · **As of:** 2026-08-20
**Extends:** `DEMONOLOGY_DESIGN_V2.md` (governing design) · resolves `DEMONOLOGY_STATUS.md §4`
**Scope:** the three ❌🔧 nodes. No new buttons; all three modify what existing baseline
casts (Life Tap, Corruption, Demonic Circle) mean to the legion. Markers unchanged.

Shared rule for all three: **each effect's qualification path is an explicit allowlist**
(§A.5). Nothing routes through the general demon-damage multiplier stack unless stated.
This is the fb/290515-class-of-bug prevention measure.

---

## A.1 Vital Conduit (vc) — T3, 2 ranks, marker `290923` *(bt prereq — build first)*

**Effect:** *Your Life Tap also heals your commanded demons for 50/100% of the health you
sacrifice, divided among them.*

Identity: the vc→bt column becomes a circulatory system — you feed the legion (vc), the
legion feeds you (bt). The life-for-power trade is preserved: the heal is paid for in the
warlock's own health, every press.

| Property | Value |
|---|---|
| Trigger | Life Tap chain (rank-1 anchor `1454`; resolve full chain from `spell_dbc` at startup). Dark Pact excluded. |
| Heal amount | `healthPaid × VitalConduit.HealPct[rank]` (0.50 / 1.00), where `healthPaid` = the health actually removed by the Tap (post-modifiers, e.g. Glyph of Life Tap doesn't change the cost and is irrelevant). |
| Recipients | Anchor + legionnaires + active greater demon, **alive and missing health**. Wild Imps excluded (short-lived; healing them is waste by construction). |
| Split | Evenly among eligible (injured) recipients. If none injured: no heal, Life Tap otherwise unaffected. Overheal on a recipient is wasted (no redistribution — keep it deterministic and cheap). |
| Heal spell | New internal ID **`290506` — Vital Conduit** (2905xx block), cast by the warlock on each recipient, so the combat log reads correctly and the amount is set dynamically. |
| Conf | `VitalConduit.HealPct.R1`, `.R2` |

**Qualification rules:**
- The heal is a *heal*, not demon damage → feeds **nothing**: no Soul Harvest, no
  Doombrand, no bt, no gb proc rolls.
- Not scaled by +healing modifiers or the demon damage multiplier stack — the amount is
  computed directly from `healthPaid` and set via `SetHitHeal`/equivalent.
- Interacts with **fc/cv** only in the trivial sense that bigger demon health pools waste
  less overheal.

**Implementation:**
1. SpellScript on the Life Tap chain, `AfterCast` (health is paid unconditionally by
   then): read the health cost from the spell's effect value as applied.
2. Query the pool manager for eligible recipients (the same iteration Command Demon's
   dispatcher uses); filter alive + `GetHealth() < GetMaxHealth()`.
3. Per recipient: warlock casts `290506` (triggered, no cost/GCD) with the split amount.
4. `.legion dumpstats`: add last-Tap line (health paid, recipients, per-demon heal).

**Acceptance:** Tap with 2 injured legionnaires at rank 2 → each receives ½ of health
paid; log shows `290506` from the warlock; Soul Harvest/Doombrand counters unmoved;
Tap with full-health army → no `290506` casts.

---

## A.2 Rapid Conjuration → **Fel Corruption** (rc) — T2, 3 ranks, marker `290909`, prereq il@2

**Effect:** *Your Corruption is infested with fel energy: its periodic damage counts as
demon damage for Soul Harvest and Doombrand at 33/66/100% effectiveness.*

Identity: the DoT is a parasite — one more demon in your service. Converts the spec's one
retained caster habit (Corruption upkeep) into legion resource flow, and completes the
T1–T2 economy column (il cheapens the spend; sh/cm/rc widen the income). Rotational
texture: Corruption uptime now matters to brand windows; refreshing before a Doombrand is
correct play; fb's killing-blow refresh becomes engine maintenance, not just damage.

| Property | Value |
|---|---|
| Trigger | Corruption chain only (rank-1 anchor `172`; resolve chain at startup). Seed of Corruption and other DoTs excluded. |
| Rank effectiveness | `FelCorruption.RankEffectiveness` = 0.33 / 0.66 / 1.00 |
| Soul Harvest | Each tick rolls the SH proc at `baseChance × rankEff`, respecting the existing per-player ICD unchanged. cm's crit acceleration does **not** apply (Corruption ticks don't crit through the demon-crit path). |
| Doombrand | If the tick's target carries this owner's `290504`: `stored += tickDmg × StorePct × FelCorruption.DoombrandStoreMult × rankEff`. `DoombrandStoreMult` = **0.5** — deliberate, per the detonation-at-cap standing alarm (§6.4 of DESIGN_V2): this adds a charging source to every window, so it charges at half weight. |
| Conf | `FelCorruption.RankEffectiveness.R1–3`, `FelCorruption.DoombrandStoreMult` |

**Qualification rules (the allowlist — this is the important part):**
- Qualifies for: **Soul Harvest proc roll** and **Doombrand accumulator**. Nothing else.
- Explicitly does NOT: trigger bt heals, roll gb or fcd procs, roll pf crits, receive
  fa/vp/cotp/fs demon multipliers or the taper, count for Beacon of Ruin, or count as a
  demon attack for any current or future "demon damage" consumer. Corruption remains
  player damage in every respect except the two named qualifications.
- Route through a dedicated helper (`LegionEconomy::QualifyPlayerPeriodic(owner, target,
  dmg, eff)`) that calls exactly `SoulHarvest::TryProc` and `Doombrand::AddCharge` —
  never through `demon_damage::Modify*`.
- Second-order note (accepted): fs's owner damage bonus and gear will grow tick size and
  therefore charge rate — that's ordinary scaling, not a loop; there is no path by which
  stored/detonated damage re-enters the accumulator.

**Implementation:**
1. AuraScript on the Corruption chain, `OnPeriodic` (post-damage-calc): read final tick
   damage, look up owner's marker rank (`290909` R1–3), call the helper.
2. Doombrand side reuses the existing accumulator entry (keyed by target GUID) and stack
   gauge — no new storage.
3. Rename in DBC talent text + `DemoTalentFix` tooltip: "Fel Corruption." The `rc` key
   and marker stay; only display text changes.
4. `.legion dumpstats`: brand line gains a "corruption-charged" running subtotal so the
   half-weight contribution is visible during tuning.

**Acceptance:** at rank 3 with a branded target, Corruption ticks visibly raise the
brand's stored subtotal at half a demon hit's rate per point of damage; SH procs occur
from ticks at the conf rate; bt heals and gb procs do NOT fire from ticks; removing the
talent stops both qualifications with no other Corruption change.

---

## A.3 Fervent Standard (fs) — T7, 2 ranks, marker `290954`

**Effect:** *Your Demonic Circle is the Legion's standard: while within 20 yards of it,
you and your commanded demons deal 4/8% increased damage, and your commanded demons take
5/10% reduced damage.*

Identity: the banner fantasy delivered through the planted object every warlock already
owns. Plant the circle where the fight will happen; the legion fights harder around it;
mobile fights forfeit it. Deliberate column with rw directly below: teleport rallies you
*and* the warped demons back inside the standard's radius in one press.

| Property | Value |
|---|---|
| Anchor object | The owner's Demonic Circle (summon spell chain anchor `48018`). No new object, no aura bookkeeping — the buff *is* three range checks. |
| Radius | 20yd (conf) — deliberately larger than melee smear so the check is about *fight location*, not micro-positioning. |
| Offense | +4/8% damage, owner and commanded demons (anchor, legionnaires, wild imps, greater demon), each unit checked individually against the circle position at damage time. |
| Mitigation | −5/10% damage taken, demons only (not the owner — the standard rallies the legion; the warlock's defense is the legion). |
| Rank split rationale | Offense low (4/8) because the spec is +25–32% ST hot and the demon half enters the multiplier stack; mitigation is the half that cannot compound with the Empowerment spine, so it carries the node's weight. |
| Conf | `FerventStandard.Radius`, `.DamagePct.R1/R2`, `.MitigationPct.R1/R2` |

**Qualification rules:**
- The **demon** offense bonus is an ordinary entry in the demon damage multiplier stack
  (like fa/vp/cotp) → it naturally flows into everything demon damage feeds (SH, brand,
  bt). **Add fs to the §10 snowball-audit list** in DESIGN_V2 for the Phase-4-style model.
- The **owner** bonus multiplies player damage → touches SH/brand only via Fel
  Corruption's tick size (accepted second-order, per §A.2).
- The mitigation is applied in the demon damage-*taken* path; it stacks multiplicatively
  with sl absorbs (absorb after mitigation — cheaper absorbs, intended synergy).

**Implementation:**
1. **Circle position cache** — hook the Demonic Circle summon `AfterCast`: store
   `{mapId, position, expiryTime}` in the pool manager (per owner). Clear on expiry
   (6 min), on re-summon (new circle replaces), on owner map change if maps differ.
   Never do a GameObject search per hit.
2. Demon offense: one range check + multiplier in the existing demon damage hook,
   gated on marker rank + valid cached circle + same map.
3. Owner offense: same check in the player damage-done hook the module already uses
   for owner-side effects (or add the standard `Unit::DealDamage` script point if not).
4. Mitigation: same check in `demon_damage::ModifySpellDamageTaken` **and the melee
   damage-taken path** — the fb bug was a spell-only check; don't repeat it here.
5. Optional polish (recommended, cheap): a 1s owner-only ticker applies/removes a
   zero-effect icon aura (`290507` — "Fervent Standard") for UI feedback when the owner
   is inside the radius. Cosmetic only; all math uses the direct range checks.
6. `.legion dumpstats`: circle line (position, time left, owner/anchor in-radius flags).

**Acceptance:** with rank 2 and a planted circle: demon hits inside radius show the
multiplier in dumpstats and lose it stepping out; melee AND spell damage taken by an
in-radius legionnaire is reduced 10%; teleporting via rw lands the warped demons inside
the radius and the bonus applies to their next hits; circle expiry cleanly ends all
three effects.

---

## A.4 Consequential decisions (formalized here)

- **il becomes a single-effect node:** the inert cast-time half is dropped (the
  cast-time fantasy is no longer wanted anywhere; rc's redesign removed the last reason
  to solve the SPELLMOD blocker). Delete the dead `ImprovedLegion.CastReductionMs` key.
  Status: il 🟡 → ✅ at build time.
- **si caster half — finish, don't park:** scale the AI recast cooldowns (imp 2s,
  Doomguard 3s, and any future caster timers) by the si haste value. This makes the
  claimed effect real for exactly the demons whose fixed timers made it fiction.
  Status: si 🟡 → ✅ at build time.
- **Banked ideas registry** (good cards surfaced by this redesign, deliberately not
  spent): passive Soul Link revival (X% owner damage split among demons); fear/seduce
  break-immunity from own-demon damage ("un-quarantines" hard CC for the one spec whose
  army made it unusable). First-refusal candidates for any future node opening.

## A.5 Cross-cutting qualification table (paste into DESIGN_V2 near §10)

| Effect | Is demon damage? | Feeds SH | Charges brand | Triggers bt/gb/fcd/pf | In demon mult stack |
|---|---|---|---|---|---|
| vc heal (`290506`) | No (heal) | No | No | No | No |
| Fel Corruption ticks | No — allowlisted only | Yes (×rankEff) | Yes (×0.5×rankEff) | No | No |
| fs owner bonus | No (player dmg mult) | Indirect via tick size only | Indirect via tick size only | No | No |
| fs demon bonus | Multiplies demon dmg | Yes (naturally) | Yes (naturally) | Yes (naturally) | **Yes — add to snowball audit** |
| fs mitigation | Damage-taken side | No | No | No | No |

## A.6 Doc deltas

- **DESIGN_V2 §4:** rewrite rc/vc/fs rows with the effects above; rename rc's display
  text to Fel Corruption; il row loses the cast-time clause.
- **DESIGN_V2 §3 (ID registry):** add `290506` Vital Conduit heal, `290507` Fervent
  Standard icon aura (optional).
- **DESIGN_V2 §10:** add fs demon bonus to the snowball list; paste §A.5.
- **STATUS doc §2:** on build, rc/vc/fs ❌🔧 → ✅; il and si 🟡 → ✅ per §A.4.
- **STATUS doc §4/§5:** superseded by this addendum.
- **Build order:** vc first (bt prereq — ends the dead point tax), then rc, then fs
  (largest, has the cache subsystem). The fb/290515 one-liner and the two §A.4 finishes
  can ride along in whichever build goes first.
