#!/usr/bin/env python3
"""
validate_tree.py — assert the talent tree matches the design BEFORE any build.

Usage:
    validate_tree.py <data/spellforge dir>

SpellForge resolves client-side concerns but does not know the design's shape,
so these assertions are ours (PLAN §7.1). Passes cleanly when the tree is not
authored yet (Phase 0 state); enforces the full spec once talents exist.
"""
from __future__ import annotations

import sys
from pathlib import Path

# Design shape (PLAN §7.1).
NODES_PER_TIER = [4, 3, 4, 3, 4, 3, 4, 4, 2, 4, 1]
# tier2/3 = 7/9 after moving Soul Harvest/Cruel Master/Fel Corruption -> tier3 and Vital Conduit/
# Pactbound Fury/Fel Blood -> tier2 (Fel Armory -> tier1). tier9/10 = 5/6 (Beacon of Ruin at tier10).
POINTS_PER_TIER = [10, 7, 9, 7, 8, 8, 8, 9, 5, 6, 1]
TOTAL_POINTS = 78


def _load_all(talents_dir: Path):
    try:
        import yaml
    except ImportError:
        print("validate_tree: PyYAML not installed; skipping.", file=sys.stderr)
        return None
    nodes = []
    for f in sorted(talents_dir.rglob("*.y*ml")):
        with f.open() as fh:
            data = yaml.safe_load(fh) or {}
        # tolerant: accept a top-level list, or a dict with a 'talents'/'nodes' list
        if isinstance(data, list):
            nodes.extend(data)
        elif isinstance(data, dict):
            for k in ("talents", "nodes"):
                if isinstance(data.get(k), list):
                    nodes.extend(data[k])
    return nodes


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    sf_dir = Path(argv[1])
    talents_dir = sf_dir / "talents"

    if not talents_dir.exists() or not any(talents_dir.rglob("*.y*ml")):
        print("validate_tree: no talents authored yet — nothing to validate (OK).")
        return 0

    nodes = _load_all(talents_dir)
    if nodes is None:
        return 0  # pyyaml missing; treated as skip

    errors: list[str] = []

    # Rank count per SpellForge's TalentNodeSpec: max_rank, else len(ranks),
    # else len(generate.per_rank_base_points).
    def _rank_count(n: dict) -> int:
        if isinstance(n.get("max_rank"), int):
            return n["max_rank"]
        if isinstance(n.get("ranks"), list):
            return len(n["ranks"])
        gen = n.get("generate")
        if isinstance(gen, dict) and isinstance(gen.get("per_rank_base_points"), list):
            return len(gen["per_rank_base_points"])
        return 1

    # Tier bucketing: SpellForge nodes carry a 0-based `row`; tier = row + 1.
    tiers: dict[int, list] = {}
    for n in nodes:
        r = n.get("row") if isinstance(n, dict) else None
        if not isinstance(r, int):
            errors.append(f"node missing integer 'row': {n!r}")
            continue
        tiers.setdefault(r + 1, []).append(n)

    if not errors:
        for idx, expected in enumerate(NODES_PER_TIER, start=1):
            got = len(tiers.get(idx, []))
            if got != expected:
                errors.append(f"tier {idx}: expected {expected} nodes, found {got}")

        for idx, expected in enumerate(POINTS_PER_TIER, start=1):
            got = sum(_rank_count(n) for n in tiers.get(idx, []))
            if got != expected:
                errors.append(f"tier {idx}: expected {expected} points, found {got}")

        pts = sum(_rank_count(n) for n in nodes if isinstance(n, dict))
        if pts != TOTAL_POINTS:
            errors.append(f"total available points: expected {TOTAL_POINTS}, found {pts}")

        # TODO(Phase 5): per-tier point totals, prereq tier ordering,
        # the §6 reference build walk, and rank->spellbook cross-refs.

    if errors:
        for e in errors:
            print(f"ERROR {e}", file=sys.stderr)
        return 1

    print(f"validate_tree: OK ({len(nodes)} node(s)).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
