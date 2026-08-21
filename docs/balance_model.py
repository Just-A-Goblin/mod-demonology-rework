#!/usr/bin/env python3
"""
Warlock spec DPS model — Affliction / Destruction / Demonology(rework).

NOT a validated combat sim. It is a transparent expected-value model:
 - Demonology numbers derive from the module's live config (data/spellforge + .conf).
 - Affliction/Destruction use ESTIMATED 3.3.5 rank-at-level values, then a per-level
   `stock_cal` anchors them to published Wrath DPS shapes (my component formulas under-count
   Shadow Embrace / Drain Soul / refunds / Molten Core etc.). Demonology is NOT calibrated.
So the meaningful output is the demo-vs-stock RATIO; stock absolutes are anchored via stock_cal.
Treat everything as +/-20%. The point is relative ranking, scaling shape, and outlier-finding.

Usage: python3 docs/balance_model.py       (tweak GEAR / DEMO and re-run)
"""

SPELL_CRIT_MULT = 1.5

# ---------------------------------------------------------------- gear per level
# has_* = level availability. At 60 you have exactly 51 talent points, so the 51-pt CAPSTONE
# TALENTS (Haunt, Chaos Bolt; also Metamorphosis which the demo rework CUT) ARE available.
# Only TRAINED spells gated above 60 are missing: Incinerate (64), Seed of Corruption (70).
# `rank` scales the level-gated nuke BASE damage (rank-1 capstones @60 hit softer than @80).
GEAR = {
    60: dict(SP=300,  crit=0.12, haste=0.00, rank=0.45, has_haunt=True,  has_seed=False,
             has_incinerate=False, has_chaosbolt=True,  stock_cal=1.00),
    80: dict(SP=2500, crit=0.33, haste=0.15, rank=1.00, has_haunt=True,  has_seed=True,
             has_incinerate=True,  has_chaosbolt=True,  stock_cal=1.35),
}

def scrit(dps, g): return dps * (1 + g['crit']*(SPELL_CRIT_MULT-1))

# ============================================================ AFFLICTION
def affliction(g, targets):
    SP = g['SP']
    corr = (180 + 0.16*SP)*6/18
    coa  = (113 + 0.10*SP)*12/24
    ua   = (230 + 0.20*SP)*5/15
    dot = scrit(corr+coa+ua, g) * 1.25
    if g['has_haunt']: dot *= 1.20
    sb = scrit(510 + 0.857*SP, g) / (2.5/(1+g['haste'])) * 1.10
    haunt_nuke = scrit(0.9*(510*g['rank']+0.857*SP), g)/8 if g['has_haunt'] else 0
    if targets == 1:
        out = dot + sb*0.55 + haunt_nuke
    else:
        base = dot*min(targets,4.5) + sb*0.10
        if g['has_seed']:
            seed = scrit(1518 + 0.30*SP, g)/(2.5/(1+g['haste']))
            base += seed*min(targets,5)*0.5
        out = base
    return out * g['stock_cal']

# ============================================================ DESTRUCTION
def destruction(g, targets):
    SP = g['SP']
    immo = scrit(258 + 0.65*SP, g)/15 + scrit(460+0.2*SP, g)/15
    conflag = scrit(0.4*(258+0.2*SP), g)/10
    if g['has_incinerate']:
        st = scrit(582 + 0.71*SP, g)*1.10/(2.5/(1+g['haste']))*(1+g['crit']*0.5) + immo + conflag
    else:
        sb = scrit(510+0.857*SP, g)*1.10/(2.3/(1+g['haste']))*(1+g['crit']*0.5)
        st = sb + immo + conflag
    if g['has_chaosbolt']:
        st += scrit(1429*g['rank'] + 0.71*SP, g)/12
    if targets == 1:
        out = st
    else:
        rof = scrit(0.257*SP + 100, g)
        out = rof*targets + immo*0.6 + (st*0.15 if g['has_chaosbolt'] else 0)
    return out * g['stock_cal']

# ============================================================ DEMONOLOGY (rework)
# Synced to LIVE config 2026-08-20 (post FLATTEN balance pass + Savage Instincts caster half).
DEMO = dict(
    VP_SPELL=0.15, VP_MELEE=0.24, FEL_ARMORY=0.15, SI_HASTE=0.12, SI_CASTER_FRAC=0.5, PF_CRIT=0.06,
    CRUELTY=0.15, EMP_DMG=0.20, EMP_UPTIME=0.60, GB_DMG=0.12, GB_UPTIME=0.30,
    BEACON=0.30, BEACON_MULT=True,
    FS=0.08,                       # Fervent Standard demon dmg while in-circle (assume active for ST)
    STORE=0.35, CAP_COEF=5.0, BRAND=10.0, BRAND_CD=20.0,
    DOOMBOLT_BASE=220, DOOMBOLT_COEF=0.55,   # FLATTEN
    WILDIMP_COEF=0.11,             # temporary Wild Imps (Summon Wild Imps) — FLATTEN
    IMPLEGION_COEF=0.20,           # permanent Imp legionnaires (decoupled) — FLATTEN
    FIREBOLT_BASE=25, FIREBOLT_CADENCE=2.0,  # FLATTEN raised the flat base
    N_PERM_IMPS=4, N_WILD=3, WILD_UPTIME=0.70, WARLOCK_FILLER=0.55,
)

def _emp(d): return (1+d['EMP_DMG']*d['EMP_UPTIME'])*(1+d['CRUELTY']*d['EMP_UPTIME'])
def _gb(d):  return (1+d['GB_DMG']*d['GB_UPTIME'])
# Savage Instincts CASTER half: scales the AI recast WAIT only (not the cast time), so a casting
# demon gains ~half the haste value in throughput. SI_CASTER_FRAC=0.5 approximates that split.
def _si_cast(d): return 1.0 + d['SI_HASTE']*d.get('SI_CASTER_FRAC', 0.5)

def firebolt_dps(g, d, coef):
    raw = (d['FIREBOLT_BASE'] + coef*g['SP'])/d['FIREBOLT_CADENCE']
    mult = (1+d['VP_SPELL'])*(1+d['FEL_ARMORY'])*(1+d['FS'])*(1+g['crit']*0.5+d['PF_CRIT']*0.5)
    return raw*mult*_emp(d)*_gb(d)*_si_cast(d)

def doombolt_dps(g, d):
    raw = (d['DOOMBOLT_BASE'] + d['DOOMBOLT_COEF']*g['SP'])/3.0
    mult = (1+d['VP_SPELL'])*(1+d['FEL_ARMORY'])*(1+d['FS'])*(1+g['crit']*0.5+d['PF_CRIT']*0.5)*_emp(d)*_si_cast(d)
    if d['BEACON_MULT']: mult *= (1 + d['BEACON'])
    else:               raw *= (1 + d['BEACON'])
    return raw*mult

def warlock_filler(g, d):
    return scrit(510+0.857*g['SP'], g)/(2.5/(1+g['haste'])) * d['WARLOCK_FILLER']

def demonology(g, d, targets):
    fb_perm = firebolt_dps(g, d, d['IMPLEGION_COEF'])   # permanent imp legionnaires
    fb_wild = firebolt_dps(g, d, d['WILDIMP_COEF'])     # temporary Wild Imps
    wild = d['N_WILD']*fb_wild*d['WILD_UPTIME']
    demonsA = d['N_PERM_IMPS']*fb_perm + wild                    # full legionnaire (imp army) build
    demonsB = doombolt_dps(g, d) + fb_perm + wild               # greater-demon (Doomguard) build
    demon_st = max(demonsA, demonsB)
    doombrand = min(d['STORE']*demon_st*d['BRAND'], d['CAP_COEF']*g['SP'])/d['BRAND_CD']
    st = demon_st + doombrand + warlock_filler(g, d) + 30
    if targets == 1:
        return st
    imp_share = demonsA if demonsA>=demonsB else fb_perm + wild
    return st + 0.15*imp_share       # focus-fire army: only iwi 2nd-target cleaves

# ============================================================ report
def table(label, demo_cfg):
    print(f"===== {label} =====")
    for lvl in (60, 80):
        g = GEAR[lvl]
        print(f"  level {lvl} (SP {g['SP']}, crit {g['crit']*100:.0f}%, haste {g['haste']*100:.0f}%)")
        print(f"  {'tgt':>3} | {'Affl':>6} | {'Dest':>6} | {'Demo':>6} | winner")
        for t in (1,3,5):
            a=affliction(g,t); s=destruction(g,t); m=demonology(g,demo_cfg,t)
            v={'Affliction':a,'Destruction':s,'Demonology':m}; w=max(v,key=v.get)
            print(f"  {t:>3} | {a:>6.0f} | {s:>6.0f} | {m:>6.0f} | {w} +{(v[w]/sorted(v.values())[-2]-1)*100:.0f}%")
    print()

# ---- tuning levers. Each dict copies DEMO + overrides. ----
def variant(**over):
    d = dict(DEMO); d.update(over); return d

# 2026-08-20 FLATTEN (APPLIED, now folded into DEMO above): ST scaled too hard (60 +25% -> 80 +82%)
# because Demo damage is nearly pure SP*coef; raised flat base + cut coef on DoomBolt (220/0.55) and
# firebolts (base 25, impLeg 0.20, wild 0.11) -> flat curve. Then Savage Instincts' caster half was
# finished (recast timers scale with demon haste), adding ~+5-7% caster throughput. Net LIVE result:
# lvl60 ST +30% / lvl80 ST +45% (model is optimistic — real gap smaller). AoE stays weak-by-design.
NO_SI = variant(SI_HASTE=0.0)   # reference: how much the Savage Instincts caster half adds

if __name__ == "__main__":
    table("CURRENT CONFIG (live 2026-08-20: FLATTEN + Savage Instincts caster half)", DEMO)
    table("reference: same but Savage Instincts OFF", NO_SI)
