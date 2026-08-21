#!/usr/bin/env python3
"""
Opportunity-cost comparison: a greater demon vs the LEGIONNAIRES it displaces.
 - Doomguard occupies 3 command slots -> vs 3 of each legionnaire type.
 - Infernal  occupies 2 command slots -> vs 2 of each legionnaire type.
Legionnaires include signature + Command echo; greater demons include Command response + (optional)
Beacon of Ruin. Empowerment OFF. Legionnaire numbers are config-derived; greater-demon numbers are
config-derived too, but Infernal's native melee/Immolation are ESTIMATES (+/-30%).
"""
VP_MELEE=0.24; VP_SPELL=0.15; FEL=0.15; SI=0.12; DCRIT=0.11
def mmult(): return (1+VP_MELEE)*(1+FEL)*(1+DCRIT)*(1+SI)
def smult(): return (1+VP_SPELL)*(1+FEL)*(1+DCRIT*0.5)
CMD_CD=30.0

# --- legionnaire DPS (from docs/legionnaire_dps.py, post Imp bump 0.30) ---
def legion(name, SP, t):
    m=(0.15*SP)/2.0*mmult()
    if name=="Imp":       return (7+0.30*SP)/2.0*smult() + 2*30/CMD_CD*smult()
    if name=="Felguard":  return m + (1+0.15*SP)/6.0*smult()*min(3,t) + (0.15*SP*1.3*min(2,t))/CMD_CD*mmult()
    if name=="Felhunter": return m + (1+0.35*SP)/6.0*smult()
    if name=="Succubus":  return m + (1+0.25*SP)/5.0*smult() + (1+0.30*SP)/CMD_CD*smult()

# --- greater demons (from docs/anchor_dps.py) ---
def doomguard(SP,t,beacon):
    dps=((300+0.60*SP)/3 + (300+0.50*SP)/3*min(t,5) + (300+0.60*SP)/CMD_CD)*smult()
    return dps*(1+(0.30 if beacon else 0))
def infernal(SP,t,beacon):
    dps=(0.22*SP*mmult() + 0.09*SP*min(t,5)*smult() + 0.40*SP*min(t,5)/CMD_CD*smult())
    return dps*(1+(0.30 if beacon else 0))

TYPES=["Felguard","Felhunter","Succubus","Imp"]
def compare(greater_name, greater_fn, n, SP, t, beacon):
    g=greater_fn(SP,t,beacon)
    print(f"  {greater_name} ({n} slots){' +Beacon' if beacon else ''}: {g:6.0f} DPS   vs {n}x each legionnaire:")
    for ty in TYPES:
        grp=n*legion(ty,SP,t)
        verdict = f"{greater_name} +{(g/grp-1)*100:.0f}%" if g>=grp else f"{n}x{ty} +{(grp/g-1)*100:.0f}%"
        print(f"      {n}x {ty:<10} {grp:6.0f}   ->  {verdict}")
    print()

for SP,lvl in ((2500,80),(300,60)):
    for t in (1,5):
        print(f"===================== level {lvl} (SP {SP}) — {t}-target =====================")
        for beacon in (True, False):
            compare("Doomguard", doomguard, 3, SP, t, beacon)
        for beacon in (True, False):
            compare("Infernal ", infernal, 2, SP, t, beacon)
