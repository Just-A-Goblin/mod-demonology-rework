#!/usr/bin/env python3
"""
Per-legionnaire DPS model — melee/firebolt + per-type SIGNATURE on cooldown + COMMAND DEMON echo
on cooldown. From live module config. Empowerment/Doombrand/Grim Bargain are OFF (they multiply
all types ~uniformly); this is the clean ranking baseline. Numbers +/-20%.
"""
# demon multipliers (config), demon crit = 5% base + Pactbound Fury 6%
VP_MELEE=0.24; VP_SPELL=0.15; FEL=0.15; SI=0.12; DCRIT=0.11
def mmult(): return (1+VP_MELEE)*(1+FEL)*(1+DCRIT*1.0)*(1+SI)     # melee: 2x crit, SI haste
def smult(): return (1+VP_SPELL)*(1+FEL)*(1+DCRIT*0.5)            # spell: 1.5x crit, no haste (fixed cd)

# config coefficients / cooldowns (post balance-pass + decouple)
IMP_LEG_COEF=0.30; FIREBOLT_CD=2.0; FIREBOLT_BASE=7
CLEAVE_COEF=0.15; CLEAVE_CD=6.0; SBITE_COEF=0.35; SBITE_CD=6.0; LASH_COEF=0.25; LASH_CD=5.0
MELEE_PER_SP=0.15; SWING=2.0
CMD_CD=30.0   # Command Demon 45s - Dark Command 3 (-15s)
ELASH_COEF=0.30   # Succubus Empowered Lash echo (290502)

def melee(SP):        return (MELEE_PER_SP*SP)/SWING * mmult()
def sig(SP,coef,cd,hits=1): return (1 + coef*SP)/cd * smult() * hits
def firebolt(SP,hits=1):    return (FIREBOLT_BASE + IMP_LEG_COEF*SP)/FIREBOLT_CD * smult() * hits

def legion_dps(name, SP, targets):
    m = melee(SP)
    if name=="Imp":
        base = firebolt(SP)                                   # ranged, no melee
        echo = 2*(0.0 + 30 + 0.0)/CMD_CD * smult()            # Firebolt x2 vanilla: ~base only, no SP -> tiny
        sigdps = 0                                            # firebolt IS its attack
        # iwi 2nd-target could add ~20% on multi (talent) - noted, not counted
    elif name=="Felguard":
        base = m
        hits = min(3, targets)                                # Cleave chains up to 3
        sigdps = sig(SP, CLEAVE_COEF, CLEAVE_CD, hits)
        echo = (MELEE_PER_SP*SP*1.3*min(2,targets))/CMD_CD * mmult()   # Cleave echo ~weapon-based x targets
    elif name=="Felhunter":
        base = m; sigdps = sig(SP, SBITE_COEF, SBITE_CD); echo = 0     # Devour = utility, 0 dmg
    elif name=="Succubus":
        base = m; sigdps = sig(SP, LASH_COEF, LASH_CD)
        echo = (1 + ELASH_COEF*SP)/CMD_CD * smult()           # Empowered Lash echo
    elif name=="Voidwalker":
        base = m; sigdps = 0; echo = 0                        # off-tank, taunt/shield echo = 0 dmg
    return base + sigdps + echo

NAMES=["Imp","Felguard","Felhunter","Succubus","Voidwalker"]
for SP,lvl in ((300,60),(2500,80)):
    for tgts in (1,5):
        rows=sorted(((n, legion_dps(n,SP,tgts)) for n in NAMES), key=lambda x:-x[1])
        print(f"=== level {lvl} (SP {SP}), {tgts}-target, signatures+Command on CD, Emp OFF ===")
        for i,(n,d) in enumerate(rows,1):
            print(f"  {i}. {n:<11} {d:6.0f} DPS")
        print()
