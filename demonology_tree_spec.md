# Demonology: Master of the Legion — Machine-Readable Talent Spec

> Reference document for implementing the talent tree in code. All node data,
> prerequisites, tier gates, and the validated reference build are canonical here.
> Human-facing design rationale lives in `demonology_master_of_the_legion.md`.

## Constants

```
TREE_NAME         = "Demonology: Master of the Legion"
PATCH             = "3.3.5a (WotLK)"
TOTAL_POINTS_AVAIL = 78          # sum of all node max ranks incl. capstone
POINTS_TO_CAPSTONE = 51
TIER_COUNT        = 11
POINTS_PER_LEVEL  = 1            # granted from level 10
TIER_GATE(tier)   = 5 * (tier - 1)   # points that must be spent in-tree to unlock
LAYOUT            = [4,3,4,3,4,3,4,4,3,3,1]   # nodes per tier, T1..T11
```

Mastery does not exist in this patch. Scaling stats: Spell Power, Hit, Crit, Haste, Spirit/MP5 (caster); Attack Power, Armor Penetration (pet).

## Node schema

Each node: `id, name, tier, max_rank, prereq, grants_slot, is_economy`.
- `prereq`: `{node_id, ranks}` or `null` — points required in another node before this one can be spent.
- `grants_slot`: integer new Command-slot total, or `null`.
- `is_economy`: true if the node participates in the Soul Shard economy.
- A node is spendable when `total_points_spent_in_tree >= TIER_GATE(tier)` AND (`prereq == null` OR `points[prereq.node_id] >= prereq.ranks`).

## Nodes (canonical)

```json
[
  {"id":"il",  "name":"Improved Legion",       "tier":1, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"fc",  "name":"Fel Conditioning",      "tier":1, "max":3, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"sh",  "name":"Soul Harvest",          "tier":1, "max":3, "prereq":null,            "grants_slot":null, "economy":true},
  {"id":"cv",  "name":"Cursed Vitality",       "tier":1, "max":2, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"rc",  "name":"Rapid Conjuration",     "tier":2, "max":3, "prereq":{"node":"il","ranks":2}, "grants_slot":null, "economy":false},
  {"id":"fa",  "name":"Fel Armory",            "tier":2, "max":3, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"cm",  "name":"Cruel Master",          "tier":2, "max":2, "prereq":{"node":"sh","ranks":1}, "grants_slot":null, "economy":true},

  {"id":"pf",  "name":"Pactbound Fury",        "tier":3, "max":3, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"fb",  "name":"Fel Blood",             "tier":3, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"ec",  "name":"Expanded Command",      "tier":3, "max":1, "prereq":{"node":"fc","ranks":3}, "grants_slot":2,    "economy":false},
  {"id":"vc",  "name":"Vital Conduit",         "tier":3, "max":2, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"si",  "name":"Savage Instincts",      "tier":4, "max":3, "prereq":{"node":"fa","ranks":3}, "grants_slot":null, "economy":false},
  {"id":"sl",  "name":"Shadowflame Legion",    "tier":4, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"iwi", "name":"Improved Wild Imps",    "tier":4, "max":2, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"ec2", "name":"Expanded Command II",   "tier":5, "max":1, "prereq":{"node":"ec","ranks":1},  "grants_slot":3,    "economy":false},
  {"id":"vp",  "name":"Vicious Pact",          "tier":5, "max":3, "prereq":{"node":"si","ranks":3},  "grants_slot":null, "economy":false},
  {"id":"bt",  "name":"Blood Tithe",           "tier":5, "max":2, "prereq":{"node":"vc","ranks":2},  "grants_slot":null, "economy":false},
  {"id":"wl",  "name":"Warded Legion",         "tier":5, "max":2, "prereq":{"node":"fc","ranks":2},  "grants_slot":null, "economy":false},

  {"id":"dr",  "name":"Demonic Rebirth",       "tier":6, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"dc",  "name":"Dark Command",          "tier":6, "max":3, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"uv",  "name":"Unholy Vigor",          "tier":6, "max":3, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"sfg", "name":"Summon Felguard",       "tier":7, "max":1, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"wotl","name":"Wrath of the Legion",   "tier":7, "max":3, "prereq":{"node":"iwi","ranks":2}, "grants_slot":null, "economy":false},
  {"id":"gb",  "name":"Grim Bargain",          "tier":7, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"fs",  "name":"Fervent Standard",      "tier":7, "max":2, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"op",  "name":"Overlord's Presence",   "tier":8, "max":3, "prereq":{"node":"ec2","ranks":1}, "grants_slot":null, "economy":false},
  {"id":"fcd", "name":"Fel Conduit",           "tier":8, "max":2, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"rw",  "name":"Riftwalker",            "tier":8, "max":1, "prereq":null,            "grants_slot":null, "economy":false},
  {"id":"cotp","name":"Cruelty of the Pit",    "tier":8, "max":3, "prereq":{"node":"uv","ranks":3},  "grants_slot":null, "economy":false},

  {"id":"es",  "name":"Eternal Servitude",     "tier":9, "max":1, "prereq":{"node":"ec2","ranks":1}, "grants_slot":null, "economy":false},
  {"id":"re",  "name":"Ruinous Empowerment",   "tier":9, "max":3, "prereq":{"node":"cotp","ranks":3},"grants_slot":null, "economy":false},
  {"id":"bor", "name":"Beacon of Ruin",        "tier":9, "max":2, "prereq":null,            "grants_slot":null, "economy":false},

  {"id":"lc",  "name":"Legion Commander",      "tier":10,"max":1, "prereq":{"node":"es","ranks":1},  "grants_slot":4,    "economy":false},
  {"id":"se",  "name":"Supreme Empowerment",   "tier":10,"max":2, "prereq":{"node":"re","ranks":3},  "grants_slot":null, "economy":false},
  {"id":"bbb", "name":"Bound by Blood",        "tier":10,"max":2, "prereq":{"node":"lc","ranks":1},  "grants_slot":null, "economy":true},

  {"id":"gwd", "name":"Grand Warlock's Design","tier":11,"max":1, "prereq":{"node":"lc","ranks":1},  "grants_slot":null, "economy":false}
]
```

## Node effects (per-rank)

Format: `id — name (max): effect string with slash-separated per-rank values.`

```
il   — Improved Legion (2): rank1 = summon cast time -0.5s; rank2 = summon shard cost -1 (min 1).
fc   — Fel Conditioning (3): demon health +5/10/15%.
sh   — Soul Harvest (3): demon attacks have 4/8/12% chance to generate a Soul Shard; 1s ICD. Replaces Drain Soul.
cv   — Cursed Vitality (2): your Stamina +3/6%, demon Stamina +6/12%.

rc   — Rapid Conjuration (3): summon cast time -1.5/3/4.5s; rank3 = castable while moving.
fa   — Fel Armory (3): Fel Armor grants demons +5/10/15% damage.
cm   — Cruel Master (2): demon crits proc Soul Harvest at 2x chance; Soul Harvest ICD -0.25/0.5s.

pf   — Pactbound Fury (3): demon crit chance +2/4/6%. Feeds Cruel Master shard procs.
fb   — Fel Blood (2): Fel Lash damage +15/30%; demon killing blows refresh your Corruption nearby.
ec   — Expanded Command (1): +1 Command slot (total 2 demons).
vc   — Vital Conduit (2): Health Funnel +20/40% efficient; no damage penalty while channeling.

si   — Savage Instincts (3): demon attack/cast speed +4/8/12%.
sl   — Shadowflame Legion (2): Hand of Gul'dan grants demons a 15/30% SP absorb shield, 3 stacks.
iwi  — Improved Wild Imps (2): Wild Imps last +5/10s; Firebolts have 10/20% chance to hit a 2nd target.

ec2  — Expanded Command II (1): +1 Command slot (total 3 demons).
vp   — Vicious Pact (3): demons gain 8/16/24% of your SP as AP and 5/10/15% of your SP as SP.
bt   — Blood Tithe (2): demons heal you for 4/8% of damage dealt; doubled at 3+ demons.
wl   — Warded Legion (2): demons 9/18% chance to fully resist harmful spells; rank2 = immune to Fear/Charm/Polymorph.

dr   — Demonic Rebirth (2): 50/100% chance to instantly resummon a dying demon; 60s CD.
dc   — Dark Command (3): Command Demon CD -3/6/10s; also grants demons 10% haste for 6s.
uv   — Unholy Vigor (3): Demonic Empowerment duration +1/2/3s.

sfg  — Summon Felguard (1): teaches Summon Felguard (heavy melee demon, Felstorm). Talent-gated; ~level 40.
wotl — Wrath of the Legion (3): Wild Imp Firebolts 10/20/30% chance to spawn an extra Wild Imp; max 2 chains/cast.
gb   — Grim Bargain (2): Grimoire of Synergy duplicate lasts +6/12s, deals +15/30% damage.
fs   — Fervent Standard (2): Legion Standard radius +5/10yd, health +50/100%, +5/10% demon damage.

op   — Overlord's Presence (3): each active demon = +2/4/6% max health and +1.5/3/4.5% haste.
fcd  — Fel Conduit (2): demon attacks 5/10% chance to grant Conduit (next Shadow Bolt instant, no mana); 3 stacks. Does NOT change SB damage.
rw   — Riftwalker (1): Rend Veil CD -90s, rift duration +10s.
cotp — Cruelty of the Pit (3): demons under Demonic Empowerment deal +5/10/15% additional damage.

es   — Eternal Servitude (1): Doomguard/Infernal permanent while a Command slot is held; their CD -> 60s.
re   — Ruinous Empowerment (3): Demonic Empowerment grants demons 7/14/20% leech; 10/20/30% chance not to expire.
bor  — Beacon of Ruin (2): Metamorphosis (Beacon) +5/10s duration; -30/60% damage taken while rooted.

lc   — Legion Commander (1): +1 Command slot (total 4 demons).
se   — Supreme Empowerment (2): Demonic Empowerment affects ALL demons incl. temp/rift; duration +3/6s.
bbb  — Bound by Blood (2): on demon death, other demons gain +15/30% damage and +25/45% haste for 10s; death refunds 1 Soul Shard.

gwd  — Grand Warlock's Design (1): [capstone] at 3+ demons, party/raid gains Legion Aura (+5% dmg, +5% haste);
       Demonic Empowerment costs no GCD; first summon in combat is free+instant; each demon = +4% to your other demons' damage.
```

## Prerequisite graph (edges: dependent <- required@ranks)

```
rc   <- il@2
cm   <- sh@1
ec   <- fc@3
si   <- fa@3
ec2  <- ec@1
vp   <- si@3
bt   <- vc@2
wl   <- fc@2
wotl <- iwi@2
op   <- ec2@1
cotp <- uv@3
es   <- ec2@1
re   <- cotp@3
lc   <- es@1
se   <- re@3
bbb  <- lc@1
gwd  <- lc@1
```

Named chains:
- **Command spine:** fc(3) -> ec -> ec2 -> es -> lc -> gwd. `op` and `bbb` also hang off ec2/lc.
- **Empowerment spine:** uv(3) -> cotp(3) -> re(3) -> se(2).
- **Demon scaling:** fa(3) -> si(3) -> vp(3).
- **Sustain:** vc(2) -> bt(2).
- **Economy:** sh(3) -> cm(2) [crit-scaling booster]; bbb refunds on death.

## Command slots

```
base_slots = 1
slots = 1 + (ec>0) + (ec2>0) + (lc>0)   # max 4
```
Temporary demons (Wild Imps, Rend Veil spawns, Grimoire duplicate) do NOT consume slots.

## Validated reference build (51 pts, raid PvE)

```
fc:3, sh:3,            # T1  (6)
fa:3, cm:2,            # T2  (5)  cum 11
ec:1, vc:2, pf:2,      # T3  (5)  cum 16
si:3, iwi:2,           # T4  (5)  cum 21
ec2:1, vp:3, bt:1,     # T5  (5)  cum 26
uv:3, dc:2,            # T6  (5)  cum 31
sfg:1, wotl:3, fs:1,   # T7  (5)  cum 36
op:3, cotp:3,          # T8  (6)  cum 42
es:1, re:3,            # T9  (4)  cum 46
lc:1, se:2, bbb:1,     # T10 (4)  cum 50
gwd:1                  # T11 (1)  cum 51
```
All tier gates cleared, all prereqs satisfied. 20 points remain for a second tree.

## Validation rules (for a linter/allocator)

1. `sum(points.values()) <= level - 9` and `<= 71` at level 80.
2. For each node with points > 0: `sum_spent_before_this_tier_is_reachable`, i.e. `total_spent >= TIER_GATE(node.tier)` must hold at allocation time.
3. For each node with points > 0 and a prereq: `points[prereq.node] >= prereq.ranks`.
4. `points[node] <= node.max` for all nodes.
5. Removing a point must not orphan a dependent (any node with points > 0 whose prereq would drop below its ranks) — reject the removal or cascade it.
6. `gwd` requires `total_spent >= 50` (tier gate) AND `points[lc] == 1`.

## Deconfliction (names cleared vs live WotLK trees)

These renames avoid collisions with existing Affliction/Destruction/Demonology talents and spells:
```
Improved Corruption   -> Fel Blood
Demonic Embrace       -> Cursed Vitality
Molten Core           -> Fel Conduit
Fel Synergy           -> Blood Tithe
Nether Ward           -> Warded Legion
Soul Siphon           -> Vital Conduit
Demonic Aegis         -> Fel Armory
Master Summoner       -> Rapid Conjuration
Demonic Brutality     -> Savage Instincts
Demonic Tactics       -> Pactbound Fury
Fel Vitality (spell)  -> Feed the Pit
```
Summon Felguard is retained as a talent (Tier 7), NOT baseline, so it stays Demonology-exclusive.

## Density vs live trees

```
Affliction   70 avail / 51 spent / 19 skipped (27%)
Destruction  75 avail / 51 spent / 24 skipped (32%)
This tree    78 avail / 51 spent / 27 skipped (35%)
```

## Known open design items (not bugs)

- **Command Pool baseline threat rule:** demons beyond the first generate 50% less threat while in a party (baseline, not a talent — avoids a talent tax). Confirmed as the chosen fix for dungeon pull problems; ensure implementation reflects this rather than a talent node.
- Mid-game levels ~35–54 are multiplier-heavy; Summon Felguard at ~40 is the main new-ability beat in that window.
