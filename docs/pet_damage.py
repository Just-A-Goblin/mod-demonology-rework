#!/usr/bin/env python3
"""
Standard-pet damage analysis for mod-demonology-rework.
Models the SUSTAINED DPS each pet type deals AS A LEGIONNAIRE (the part the module fully
controls & scales), from live config. Anchor-pet (core Pet) native abilities differ and are
noted separately. Empowerment/Doombrand/Grim Bargain are OFF here (they multiply ALL pets
~+20-40%); this is the clean baseline. Numbers +/-15%.
"""
# ---- module config (post balance-pass) ----
MELEE_PER_SP   = 0.15    # Inherit.MeleeDamagePerSP  (all melee legionnaires)
FIREBOLT_COEF  = 0.16    # WildImp.SPCoefficient      (Imp legionnaires only)
FIREBOLT_BASE  = 7
ATTACK_TIME    = 2.0     # BaseAttackTime all pets
FIREBOLT_CD    = 2.0     # fixed AI cadence (haste does NOT speed it)
VP_MELEE=0.24; VP_SPELL=0.15; FEL_ARMORY=0.15; SI_HASTE=0.12; PF_CRIT=0.06

def melee_dps(SP, crit):
    raw = (MELEE_PER_SP*SP)/ATTACK_TIME
    crit_mult = 1 + (crit + 0.05 + PF_CRIT)*(2.0-1)      # melee crit = 2x
    haste = 1 + SI_HASTE                                  # Savage Instincts speeds swings
    return raw * (1+VP_MELEE) * (1+FEL_ARMORY) * crit_mult * haste

def firebolt_dps(SP, crit):
    raw = (FIREBOLT_BASE + FIREBOLT_COEF*SP)/FIREBOLT_CD  # fixed cadence, no haste
    crit_mult = 1 + (crit + PF_CRIT)*(1.5-1)             # spell crit = 1.5x
    return raw * (1+VP_SPELL) * (1+FEL_ARMORY) * crit_mult

PETS = [
    ("Imp",       "Firebolt (ranged, 0.16xSP)"),
    ("Succubus",  "melee only (Lash unused)"),
    ("Voidwalker","melee only (Torment unused)"),
    ("Felhunter", "melee only (Shadow Bite unused)"),
    ("Felguard",  "melee only (Felstorm unused)"),
]

for SP, crit, lvl in ((300,0.12,60),(2500,0.33,80)):
    print(f"=== LEGIONNAIRE sustained DPS @ level {lvl} (SP {SP}, crit {crit*100:.0f}%), Empowerment OFF ===")
    print(f"  {'pet':<11} | {'mechanic':<30} | {'DPS':>6} | effective SP-coef")
    for name, mech in PETS:
        if name == "Imp":
            dps = firebolt_dps(SP, crit); eff = (dps*FIREBOLT_CD)/SP
        else:
            dps = melee_dps(SP, crit);    eff = (dps*ATTACK_TIME)/SP
        print(f"  {name:<11} | {mech:<30} | {dps:>6.0f} | {eff:>5.2f}")
    print()
