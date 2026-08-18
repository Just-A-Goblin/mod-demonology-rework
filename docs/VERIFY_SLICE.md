# Verifying the vertical slice in-game

The slice (Soul Harvest 3/3 + Summon Wild Imps + Demonic Empowerment) is built
and vendored into `dist/`, and the C++ is wired into the loader. What remains is a
worldserver rebuild + deploy + in-game check — the worldserver was stopped during
authoring, so this could not be exercised live yet.

## Deploy

```bash
# from the module dir, with the worldserver stopped
./install.sh --apply           # SERVER side: patches, module link, DBC->server, SQL->DBs
                               #   (also runs the client step below if SpellForge is present)
./deploy_client.sh --apply     # CLIENT side: merge our DBCs into the winning client patch
```

Then **rebuild the worldserver** (the module adds new C++) and restart it.

### Why the client needs its own step

On this client a higher-priority patch (`patch-V.mpq`, letters outrank numbers)
already supplies `Spell.dbc`, so a plain standalone patch would be **ignored**.
`deploy_client.sh` uses SpellForge to merge our DBCs *into* `patch-V.mpq` in place
(preserving its other content) — verified to produce a `patch-V.mpq` whose
`Spell.dbc` carries our 7 spells. This merge reads the target client's MPQ, so it
can't be pre-baked into `dist/`; it runs against the real client at deploy time.
`dist/dbc/` holds our portable DBC payload; `dist/mpq/patch-6.MPQ` is only useful
on a client with no overriding patch.

## In-game checks (GM account)

```
.learn 290001        # Summon Wild Imps
.learn 290000        # Demonic Empowerment
.learn 290012        # Soul Harvest (Rank 3)  -> or 290010 / 290011 for rank 1/2
```

| Test | Expected |
|---|---|
| Cast **Summon Wild Imps** (290001) | 3 "Wild Imp" (npc 600000) appear for 20s and attack your target |
| `.legion pool` / `.legion dumpstats` | GM command namespace responds |
| Attack a dummy with imps out, watch bags | Soul Shards (item 6265) trickle in — throttled ~1/s per the ICD |
| `.legion shards` | reports current Soul Shard count |
| Cast **Demonic Empowerment** (290000) with imps/pet out | imps + pet gain the `Demonic Empowerment` buff (haste + damage), 12s |

## Known slice simplifications (by design, not bugs)

- Soul Harvest is a **learnable passive** here; its talent-tree placement is Phase 5
  (replacing the Demonology tab wholesale is deferred).
- Wild Imps are summoned owned guardians that melee/attack the caster's target;
  full mirroring AI + Firebolt casting + SP inheritance are Phase 1/3/6.
- Buff/economy numbers are placeholders (config-driven), not tuned.
