#!/usr/bin/env python3
"""
Anchor pet + greater demon DPS model, with Command Demon on cooldown.

IMPORTANT HONESTY: anchor pets (Imp/Succ/VW/Felhunter/Felguard) use CORE native abilities + core
stat-inheritance, which this module does NOT control, so their damage here is an ESTIMATE from
WotLK pet relativities expressed as a native SP-coefficient (DPS-per-SP before our mults). Treat
anchor absolutes as +/-30%. The GREATER DEMONS (Doomguard/Infernal) are config-derived (Doom Bolt/
Blast, Command Pulse, Beacon of Ruin) and more reliable. Empowerment OFF (multiplies all).
"""
VP_MELEE=0.24; VP_SPELL=0.15; FEL=0.15; SI=0.12; DCRIT=0.11
def mmult(): return (1+VP_MELEE)*(1+FEL)*(1+DCRIT*1.0)*(1+SI)
def smult(): return (1+VP_SPELL)*(1+FEL)*(1+DCRIT*0.5)
CMD_CD=30.0            # Command Demon 45s - Dark Command 3

# --- ESTIMATED anchor native SP-coefficients (DPS per SP, pre-our-mults) ---
NATIVE = dict(Felguard=0.26, Felhunter=0.16, Succubus=0.16, Imp=0.14, Voidwalker=0.06)
IMP_ANCHOR_BONUS=0.30                      # config: spec-agnostic Imp anchor buff

# --- greater demons (config) ---
DBOLT_BASE=300; DBOLT_COEF=0.60           # Doom Bolt (per 3s)
DBLAST_BASE=300; DBLAST_COEF=0.50         # Doom Blast (AoE, triggered each bolt)
INF_MELEE_COEF=0.22; INF_IMMOLATE_COEF=0.09   # ESTIMATED Infernal native melee + Immolation aura
INF_PULSE_COEF=0.40                        # Infernal Command Pulse (config)
BEACON=0.30                                # Beacon of Ruin (greater demons)

def anchor(name, SP, targets):
    base = NATIVE[name]*SP
    if name=="Imp":
        dps = base*smult()*(1+IMP_ANCHOR_BONUS)
        dps += 3*(30)/CMD_CD*smult()                 # Firebolt volley x3 (vanilla base only)
    else:
        dps = base*mmult()
        if name=="Felguard":
            dps += (0.12*SP*min(2,targets))/CMD_CD*mmult()   # Intercept+Cleave signature (~weapon)
    return dps

def doomguard(SP, targets):
    bolt = (DBOLT_BASE + DBOLT_COEF*SP)/3.0
    blast = (DBLAST_BASE + DBLAST_COEF*SP)/3.0 * min(targets,5)   # AoE cleave each bolt
    cmd = (DBOLT_BASE + DBOLT_COEF*SP)/CMD_CD                     # extra bolt per press
    return (bolt+blast+cmd)*smult()*(1+BEACON)

def infernal(SP, targets):
    melee = INF_MELEE_COEF*SP*mmult()
    immol = INF_IMMOLATE_COEF*SP*min(targets,5)*smult()          # Immolation aura = AoE
    cmd = (INF_PULSE_COEF*SP)*min(targets,5)/CMD_CD*smult()      # Command Pulse (fire AoE)
    return (melee+immol+cmd)*(1+BEACON)

def all_dps(SP, targets):
    d = {n: anchor(n,SP,targets) for n in NATIVE}
    d["Doomguard(greater)"]=doomguard(SP,targets)
    d["Infernal(greater)"]=infernal(SP,targets)
    return d

for SP,lvl in ((300,60),(2500,80)):
    for t in (1,5):
        rows=sorted(all_dps(SP,t).items(), key=lambda x:-x[1])
        print(f"=== level {lvl} (SP {SP}), {t}-target, Command on CD, Emp OFF ===")
        for i,(n,v) in enumerate(rows,1):
            print(f"  {i}. {n:<20} {v:6.0f} DPS")
        print()
