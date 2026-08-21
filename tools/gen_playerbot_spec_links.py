#!/usr/bin/env python3
"""
gen_playerbot_spec_links.py — emit mod-playerbots PremadeSpecLink strings for the reworked
Demonology tree, generated (never hand-written) from the shipped Talent.dbc + authoring YAML.

WHY THIS EXISTS
    mod-playerbots picks bot talents from `AiPlayerbot.PremadeSpecLink.<cls>.<spec>.<level>` strings.
    Each string is `<affli>-<demo>-<destro>`, where a tab's segment is one DIGIT PER TALENT, and the
    digit is the TARGET RANK of the talent at grid position i. Position i is the i-th talent when the
    tab's talents are sorted by (Row asc, then Col asc) — exactly how PlayerbotAIConfig::
    ParseTempTalentsOrder orders them, and how PlayerbotFactory::InitTalentsBySpecNo applies them
    (nearest breakpoint <= bot level, talents filled in row/col order, capped by free points, with
    each talent's DBC DependsOn auto-learned first).

    A digit in the wrong position silently mis-assigns talents; a rank over a talent's max is ignored.
    So the string is 100% positional against the SHIPPED DBC — this tool reads that DBC, validates the
    build (prereqs, tier gates, max ranks, point totals) at EVERY leveling breakpoint, and refuses to
    emit anything if a single check fails. Output goes to conf/playerbots-demonology.conf.fragment,
    which install_playerbots.sh merges into the live playerbots.conf inside a managed marker block.

VERIFIED GROUND TRUTH (mod-playerbots @ 085e127e, this server's DBC)
    - Warlock class = 9; classMask 256. Demonology = TalentTab 303, tabpage 1 => demo is SEGMENT
      INDEX 1 (the part between the two dashes). Spec index 1 = "demo pve" (RandomClassSpecProb 34).
    - Talent.dbc: 36 demo talents, standard 3.3.5a layout (23 fields / 92 bytes).
    - Digit = target rank (LearnTalent(id, min(digit, free)-1)); breakpoint search walks DOWN to the
      nearest non-empty level, then applies upward until talent points are exhausted.

BUILD: the level-80, 71-point single-target legionnaire build signed off in
       docs/BOT_INTEGRATION_ANALYSIS.md (Felguard anchor + 3 Imp legionnaires; cuts: cv, fb, es, bor).

Run:  python3 tools/gen_playerbot_spec_links.py           # validate + write the fragment
      python3 tools/gen_playerbot_spec_links.py --check    # validate + print, write nothing
"""
import os, sys, struct, re

# ----- pinned provenance (for the fragment header / MANIFEST) -----
PLAYERBOTS_PIN = "085e127e38bcc9952338e40e45a8a22472585502"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TALENT_DBC   = os.path.join(REPO, "dist", "dbc", "Talent.dbc")
TALENTTAB_DBC= os.path.join(REPO, "dist", "dbc", "TalentTab.dbc")
TREE_YAML    = os.path.join(REPO, "data", "spellforge", "talents", "demonology_tree.yaml")
OUT_FRAGMENT = os.path.join(REPO, "conf", "playerbots-demonology.conf.fragment")
# the pinned playerbots .dist we read stock warlock links from (to neutralize demo-tab dips)
PB_DIST = "/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore/modules/mod-playerbots/conf/playerbots.conf.dist"

WARLOCK_CLASS   = 9
WARLOCK_MASK    = 1 << (WARLOCK_CLASS - 1)   # 256
DEMO_TAB_ID     = 303
DEMO_TABPAGE    = 1                          # 0 affli, 1 demo, 2 destro
DEMO_SPEC_INDEX = 1                          # AiPlayerbot spec index for "demo pve"
TALENT_POINTS_PER_TIER = 5                   # tier gate: row R requires 5*R points spent in the tab
FIRST_TALENT_LEVEL = 10                      # talent point count at level L == L - 9
MAX_BREAKPOINT_LEVEL = 80


def die(msg):
    sys.stderr.write("ERROR: " + msg + "\n")
    sys.exit(1)


# ---------------------------------------------------------------------------
# 1. Read the authoring tree (node key -> row/col/max_rank/prereq). Source of the DESIGN.
# ---------------------------------------------------------------------------
def load_tree_yaml(path):
    """Minimal parse of demonology_tree.yaml — we only need key,row,col,max_rank,requires per node.
    (Avoids a PyYAML dependency; the file is machine-generated-stable and simple.)"""
    nodes = {}
    cur = None
    with open(path) as f:
        for raw in f:
            line = raw.rstrip("\n")
            m = re.match(r"\s*- key:\s*demonology\.(\S+)", line)
            if m:
                cur = {"key": m.group(1), "row": None, "col": None,
                       "max_rank": None, "req_key": None, "req_rank": None}
                nodes[cur["key"]] = cur
                continue
            if cur is None:
                continue
            m = re.match(r"\s+row:\s*(\d+)", line)
            if m: cur["row"] = int(m.group(1)); continue
            m = re.match(r"\s+col:\s*(\d+)", line)
            if m: cur["col"] = int(m.group(1)); continue
            m = re.match(r"\s+max_rank:\s*(\d+)", line)
            if m: cur["max_rank"] = int(m.group(1)); continue
            m = re.match(r"\s+ranks:\s*\[([^\]]*)\]", line)
            if m: cur["max_rank"] = len([x for x in m.group(1).split(",") if x.strip()]); continue
            m = re.match(r"\s+per_rank_base_points:\s*\[([^\]]*)\]", line)
            if m and cur["max_rank"] is None:
                cur["max_rank"] = len([x for x in m.group(1).split(",") if x.strip()]); continue
            m = re.match(r"\s+requires:\s*\{\s*talent:\s*demonology\.(\S+),\s*rank:\s*(\d+)\s*\}", line)
            if m: cur["req_key"] = m.group(1); cur["req_rank"] = int(m.group(2)); continue
    for k, n in nodes.items():
        if n["row"] is None or n["col"] is None or n["max_rank"] is None:
            die(f"tree node '{k}' missing row/col/max_rank in YAML")
    return nodes


# ---------------------------------------------------------------------------
# 2. Read the SHIPPED Talent.dbc / TalentTab.dbc (the bytes the server actually loads).
# ---------------------------------------------------------------------------
def read_dbc(path):
    with open(path, "rb") as f:
        d = f.read()
    magic, rc, fc, rs, ss = struct.unpack("<4sIIII", d[:20])
    if magic != b"WDBC":
        die(f"{path} is not a WDBC file")
    if rs != fc * 4:
        die(f"{path} record size {rs} != fields*4 ({fc*4})")
    recs = []
    off = 20
    for _ in range(rc):
        recs.append(struct.unpack("<%dI" % fc, d[off:off+rs]))
        off += rs
    return recs


def load_demo_talents(talent_path, talenttab_path):
    """Return {(row,col): {'id':, 'nranks':}} for the demo tab, after asserting the tab's identity."""
    # confirm TalentTab 303 is warlock/demo (fields: 0 id, ..17 name_lang, 18 icon, 19 raceMask,
    # 20 classMask, 21 petMask, 22 tabpage, 23 background)
    tab_ok = False
    for r in read_dbc(talenttab_path):
        if r[0] == DEMO_TAB_ID:
            classmask, tabpage = r[20], r[22]
            if not (classmask & WARLOCK_MASK):
                die(f"TalentTab {DEMO_TAB_ID} classMask {classmask} lacks warlock bit {WARLOCK_MASK}")
            if tabpage != DEMO_TABPAGE:
                die(f"TalentTab {DEMO_TAB_ID} tabpage {tabpage} != expected {DEMO_TABPAGE}")
            tab_ok = True
    if not tab_ok:
        die(f"TalentTab {DEMO_TAB_ID} not found in {talenttab_path}")
    # demo talents: fields 0 id,1 tab,2 row,3 col,4..12 rank[9]
    grid = {}
    for r in read_dbc(talent_path):
        if r[1] != DEMO_TAB_ID:
            continue
        nranks = len([x for x in r[4:13] if x])
        grid[(r[2], r[3])] = {"id": r[0], "nranks": nranks}
    return grid


# ---------------------------------------------------------------------------
# 3. The signed-off build + its leveling order (economy/slots first, Felguard the moment T7 unlocks).
#    Each tuple ADDS ranks to a node, processed in sequence — one talent point per rank.
#    docs/BOT_INTEGRATION_ANALYSIS.md is the human rationale; this list is the machine spec.
# ---------------------------------------------------------------------------
LEVELING_ORDER = [
    # rows 0-1: imp cost, demon damage, demon health, crit — reach 10 pts to unlock tier 3
    ("il", 2), ("fa", 3), ("fc", 3), ("pf", 2),
    # tier 3: the shard engine + first legionnaire slot
    ("sh", 3), ("ec", 1), ("rc", 3), ("cm", 2),
    # finish the row 0-1 leftovers (Vital Conduit gates Blood Tithe; last point of Pactbound Fury)
    ("vc", 2), ("pf", 1),
    # tier 4 core throughput + 2nd slot (Vicious Pact needs si@3)
    ("si", 3), ("iwi", 2), ("vp", 3), ("ec2", 1),
    # Felguard anchor the moment tier 7 opens (~L41)
    ("sfg", 1),
    # tier 5: Command haste + Empowerment duration
    ("dc", 3), ("uv", 3),
    # RUSH the Doombrand chain so it lands the instant the tier-11 gate opens at 50 pts (L60):
    #   bt/wl/dr are the cheap filler that crosses each gate; bbb (tier9 @40) -> lc (tier10 @45, the
    #   3rd slot) -> gwd (tier11 @50, Doombrand). gb/sl here are the fillers that reach 49/50.
    ("bt", 2), ("bbb", 2), ("wl", 2), ("dr", 2), ("lc", 1), ("gb", 2), ("sl", 1), ("gwd", 1),
    # L61-80: the remaining throughput + Empowerment amplifiers
    ("wotl", 3), ("sl", 1), ("fs", 2), ("cotp", 3), ("op", 3), ("fcd", 2), ("rw", 1),
    ("re", 3), ("se", 2),
]

# the exact level-80 target the order must sum to (independent check against the analysis doc)
EXPECTED_FINAL = {
    "fa": 3, "fc": 3, "il": 2, "pf": 3, "vc": 2, "sh": 3, "ec": 1, "rc": 3, "cm": 2,
    "si": 3, "iwi": 2, "sl": 2, "vp": 3, "ec2": 1, "bt": 2, "wl": 2, "dc": 3, "uv": 3,
    "dr": 2, "sfg": 1, "wotl": 3, "gb": 2, "fs": 2, "cotp": 3, "op": 3, "fcd": 2, "rw": 1,
    "re": 3, "bbb": 2, "lc": 1, "se": 2, "gwd": 1,
    # explicit cuts
    "cv": 0, "fb": 0, "es": 0, "bor": 0,
}
EXPECTED_TOTAL = 71


# ---------------------------------------------------------------------------
# 4. Validate the build against the DBC, then compute per-breakpoint rank vectors.
# ---------------------------------------------------------------------------
def validate_and_build(nodes, grid):
    # 4a. DBC <-> YAML structural agreement (positions + max ranks). Any drift => refuse.
    yaml_pos = {(n["row"], n["col"]): k for k, n in nodes.items()}
    if set(yaml_pos) != set(grid):
        die("DBC/YAML (row,col) sets differ:\n  only YAML: %s\n  only DBC: %s"
            % (sorted(set(yaml_pos) - set(grid)), sorted(set(grid) - set(yaml_pos))))
    for pos, k in yaml_pos.items():
        if nodes[k]["max_rank"] != grid[pos]["nranks"]:
            die(f"node {k} at {pos}: YAML max_rank {nodes[k]['max_rank']} != DBC ranks {grid[pos]['nranks']}")
    if len(grid) != 36:
        die(f"expected 36 demo talents, DBC has {len(grid)}")

    # 4b. Walk the leveling order, enforcing max rank, prereqs, and tier gates at every point.
    rank = {k: 0 for k in nodes}
    spent = 0
    per_point_state = []   # per_point_state[p] = dict of ranks after (p+1) points spent
    for key, add in LEVELING_ORDER:
        if key not in nodes:
            die(f"leveling order references unknown node '{key}'")
        for _ in range(add):
            n = nodes[key]
            # tier gate: need >= row*5 already spent BEFORE taking a talent in this row
            if spent < n["row"] * TALENT_POINTS_PER_TIER:
                die(f"tier gate: taking {key} (row {n['row']}) at {spent} spent, need "
                    f"{n['row']*TALENT_POINTS_PER_TIER}")
            # prereq
            if n["req_key"] and rank[n["req_key"]] < n["req_rank"]:
                die(f"prereq: {key} needs {n['req_key']} rank {n['req_rank']}, have {rank[n['req_key']]}")
            # max rank
            if rank[key] + 1 > n["max_rank"]:
                die(f"over max rank: {key} -> {rank[key]+1} > {n['max_rank']}")
            rank[key] += 1
            spent += 1
            per_point_state.append(dict(rank))

    # 4c. Totals match the signed-off build exactly.
    if spent != EXPECTED_TOTAL:
        die(f"leveling order sums to {spent} points, expected {EXPECTED_TOTAL}")
    final = {k: v for k, v in rank.items()}
    for k, want in EXPECTED_FINAL.items():
        if final.get(k, 0) != want:
            die(f"final rank mismatch for {k}: got {final.get(k,0)}, expected {want}")
    for k, v in final.items():
        if k not in EXPECTED_FINAL:
            die(f"node {k} ended at rank {v} but is not in EXPECTED_FINAL (unlisted)")
    return per_point_state


# ---------------------------------------------------------------------------
# 5. Emit the positional demo segment for a given rank vector.
# ---------------------------------------------------------------------------
def demo_segment(rank_vec, nodes):
    """36-digit string in (row,col) ascending order = the order ParseTempTalentsOrder sorts them."""
    ordered = sorted(nodes.values(), key=lambda n: (n["row"], n["col"]))
    digits = []
    for n in ordered:
        r = rank_vec.get(n["key"], 0)
        if r > 9:
            die(f"rank {r} for {n['key']} exceeds a single digit")
        digits.append(str(r))
    return "".join(digits)


def link_for_level(rank_vec, nodes):
    # pure demo build: empty affliction (leading dash), demo segment, no destro dip.
    return "-" + demo_segment(rank_vec, nodes)


# ---------------------------------------------------------------------------
# 6. Neutralize stock warlock links that dip into the demo tab (now a different 36-node tree).
# ---------------------------------------------------------------------------
def neutralize_demo_dips(dist_path):
    """Return list of (key, corrected_value) for every non-demo warlock PremadeSpecLink whose demo
    segment (index 1) is non-empty. Blanking it removes any reference to the reworked tab so those
    builds can never mis-learn our talents. Demo-pve (spec 1) is owned separately and skipped."""
    if not os.path.exists(dist_path):
        sys.stderr.write(f"WARN: {dist_path} not found; skipping demo-dip neutralization\n")
        return []
    out = []
    pat = re.compile(r"^\s*AiPlayerbot\.PremadeSpecLink\.9\.(\d+)\.(\d+)\s*=\s*(\S+)\s*$")
    with open(dist_path) as f:
        for line in f:
            m = pat.match(line)
            if not m:
                continue
            spec, level, val = int(m.group(1)), int(m.group(2)), m.group(3)
            if spec == DEMO_SPEC_INDEX:
                continue   # we own demo-pve entirely
            segs = val.split("-")
            if len(segs) > 1 and segs[1]:   # non-empty demo segment
                segs[1] = ""
                out.append((f"AiPlayerbot.PremadeSpecLink.9.{spec}.{level}", "-".join(segs)))
    return out


# ---------------------------------------------------------------------------
# 7. Assemble the fragment.
# ---------------------------------------------------------------------------
def build_fragment(per_point_state, nodes, dips):
    lines = []
    lines.append("# BEGIN mod-demonology-rework (generated by tools/gen_playerbot_spec_links.py)")
    lines.append("#")
    lines.append(f"# Source: dist/dbc/Talent.dbc (validated) + data/spellforge/talents/demonology_tree.yaml")
    lines.append(f"# Target: mod-playerbots @ {PLAYERBOTS_PIN}")
    lines.append("# Demonology = warlock (cls 9) spec index 1, tab 303 (tabpage 1). Digit = target rank,")
    lines.append("# positional in (Row,Col) order. 71-pt single-target legionnaire build; see")
    lines.append("# docs/BOT_INTEGRATION_ANALYSIS.md. DO NOT EDIT BY HAND — regenerate instead.")
    lines.append("#")
    lines.append("# Per-level breakpoints 10..80: each string sums to exactly (level-9) points, so the")
    lines.append("# factory applies it with zero truncation and the bot has precisely the intended build.")
    lines.append("")
    for level in range(FIRST_TALENT_LEVEL, MAX_BREAKPOINT_LEVEL + 1):
        points = min(level - (FIRST_TALENT_LEVEL - 1), EXPECTED_TOTAL)
        rank_vec = per_point_state[points - 1]
        link = link_for_level(rank_vec, nodes)
        # sanity: digit sum == points, every digit <= that node's max rank
        seg = link.split("-")[1]
        if sum(int(c) for c in seg) != points:
            die(f"level {level}: segment sums to {sum(int(c) for c in seg)} != {points}")
        lines.append(f"AiPlayerbot.PremadeSpecLink.9.1.{level} = {link}")
    lines.append("")
    lines.append("# --- Combat rotation tuning (read by the rework triggers in mod-playerbots) ---")
    lines.append("# Shards the Wild Imps overflow-spender must leave unspent (reserve for brand + command).")
    lines.append("AiPlayerbot.Demonology.ShardReserve = 2")
    lines.append("# How long to hold a ready Command Demon for the next Doombrand window before pressing anyway.")
    lines.append("AiPlayerbot.Demonology.CommandBrandAlignWindowMs = 4000")
    lines.append("# Don't spend a Doombrand on a target below this health %% (it would die before detonation).")
    lines.append("AiPlayerbot.Demonology.DoombrandMinTargetHealthPct = 30")
    lines.append("")
    lines.append("# --- Neutralize stock warlock builds that dipped into the (now reworked) demo tab ---")
    if dips:
        for key, val in dips:
            lines.append(f"{key} = {val}")
    else:
        lines.append("# (none found in the pinned .dist)")
    lines.append("")
    lines.append("# END mod-demonology-rework")
    return "\n".join(lines) + "\n"


def main():
    check_only = "--check" in sys.argv
    nodes = load_tree_yaml(TREE_YAML)
    grid = load_demo_talents(TALENT_DBC, TALENTTAB_DBC)
    per_point_state = validate_and_build(nodes, grid)
    dips = neutralize_demo_dips(PB_DIST)
    fragment = build_fragment(per_point_state, nodes, dips)

    # report
    sys.stderr.write("validated: 36 demo talents, DBC<->YAML aligned, 71-pt build, "
                     f"{MAX_BREAKPOINT_LEVEL - FIRST_TALENT_LEVEL + 1} breakpoints, "
                     f"{len(dips)} demo-dip(s) neutralized\n")
    sys.stderr.write("  L80 demo segment: " + demo_segment(per_point_state[-1], nodes) + "\n")

    if check_only:
        sys.stdout.write(fragment)
        return
    os.makedirs(os.path.dirname(OUT_FRAGMENT), exist_ok=True)
    with open(OUT_FRAGMENT, "w") as f:
        f.write(fragment)
    sys.stderr.write(f"wrote {OUT_FRAGMENT}\n")


if __name__ == "__main__":
    main()
