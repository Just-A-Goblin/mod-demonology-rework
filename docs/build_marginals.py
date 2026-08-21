#!/usr/bin/env python3
"""
build_marginals.py — per-talent marginal single-target DPS value, to justify the level-80 bot build.

Imports the live-synced DEMO model from balance_model.py and toggles each damage talent OFF to measure
its single-target contribution at level 80 (SP 2500). Talents that aren't in the EV damage model
(command slots, the economy chain, sustain, utility) are listed separately with a mechanic rationale —
the model can't price "another whole legionnaire" or "shards to press with," so those are argued, not
numbered. Run: python3 docs/build_marginals.py
"""
import importlib.util, sys, copy
spec = importlib.util.spec_from_file_location("bm", "docs/balance_model.py")
bm = importlib.util.module_from_spec(spec); sys.argv = ["x"]; spec.loader.exec_module(bm)

G = bm.GEAR[80]
BASE = bm.demonology(G, bm.DEMO, 1)

def st_with(**over):
    d = dict(bm.DEMO); d.update(over)
    return bm.demonology(G, d, 1)

# Each entry: label -> the DEMO override that turns the talent OFF (or to its no-talent value).
OFF = {
    "Fel Armory (fa 3/3)":        dict(FEL_ARMORY=0.0),
    "Vicious Pact (vp 3/3)":      dict(VP_SPELL=0.0, VP_MELEE=0.0),
    "Pactbound Fury (pf 3/3)":    dict(PF_CRIT=0.0),
    "Cruelty of the Pit (cotp 3)":dict(CRUELTY=0.0),
    "Grim Bargain (gb 2/2)":      dict(GB_DMG=0.0),
    "Fervent Standard (fs 2/2)":  dict(FS=0.0),
    "Savage Instincts (si 3/3)":  dict(SI_HASTE=0.0),
    "Beacon of Ruin (bor 2/2)":   dict(BEACON=0.0),   # legionnaire build has NO greater demon
    "Doombrand engine (gwd)":     dict(STORE=0.0),
    "Wild Imp throughput":        dict(WILDIMP_COEF=0.0),
    "Imp-legionnaire throughput": dict(IMPLEGION_COEF=0.0),
    # Empowerment spine uptime/amp (Demonic Empowerment is the baseline ability; the spine talents
    # extend/​amplify it). Model EMP_UPTIME 0.60; uv/se raise it, cotp/re amplify.
    "Empowerment spine uptime (uv/se)": dict(EMP_UPTIME=0.20),
}

print(f"level 80 single-target baseline (full build): {BASE:.0f} DPS\n")
print(f"{'talent':32} {'ST w/o it':>10} {'Δ DPS':>8} {'% of total':>10}")
rows = []
for label, ov in OFF.items():
    v = st_with(**ov)
    rows.append((label, v, BASE - v, (BASE - v) / BASE * 100))
for label, v, d, pct in sorted(rows, key=lambda r: -r[2]):
    print(f"{label:32} {v:>10.0f} {d:>8.0f} {pct:>9.1f}%")

print("""
NOT in the EV damage model (argue by mechanic, not number):
  MANDATORY (each is a whole extra demon or the loop itself):
    ec / ec2 / lc  — +1 legionnaire each. A legionnaire ~= one of the model's demon slots; the model
                     BAKES IN 4 demons (N_PERM_IMPS), so dropping a slot removes a full demon's DPS.
    sh / cm / rc   — the shard engine. No shards = can't press Doombrand / Command / Wild Imps at all,
                     so these gate the entire active loop (and rc feeds SH + half-weight brand).
    gwd            — grants Doombrand (see 'Doombrand engine' above for its damage).
  SUSTAIN / UTILITY (cut candidates — near-zero ST DPS):
    vc, bt         — self-heal (Life Tap heal / demon leech). Survival, not DPS.
    wl             — demon spell resist + CC immunity. Survival/utility.
    dr, bbb        — resummon-on-death / death payoff. Value scales with how often demons die.
    il             — Wild Imp mana efficiency. Sustain, not DPS (matters only if mana-starved).
    iwi, wotl      — more Wild Imp uptime/count (feeds the WILDIMP_COEF line above, indirectly).
    uv / se / re / sl — the rest of the Empowerment spine (see uptime line above; re adds leech).
""")
