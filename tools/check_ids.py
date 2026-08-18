#!/usr/bin/env python3
"""
check_ids.py — fail the build on out-of-range or colliding pinned IDs.

Usage:
    check_ids.py <module ids.yaml> [content/ids.yaml]

SpellForge spells/talents have no inline `id:` — IDs are pinned by key in the
module's ids.yaml and seeded into content/ids.yaml (see seed_ids.py). This checks
those pins sit inside the assigned blocks (docs/ID_RANGES.md) and don't collide
with a different key already in the shared registry.

Passes cleanly when nothing is pinned yet.
"""
from __future__ import annotations

import sys
from pathlib import Path

# Per-section allowed bands. All spell sub-blocks live in the same ids.yaml
# 'spells' section, so the spell check accepts the union of the three bands.
SPELL_BANDS = [(290000, 290499), (290500, 290899), (290900, 291199)]
SECTION_BANDS = {
    "spells": SPELL_BANDS,
    "talents": [(9000, 9099)],
    "spell_icons": [(19000, 19099)],
}


def _entry_id(entry):
    return entry if isinstance(entry, int) else (entry or {}).get("id")


def _in_bands(i: int, bands) -> bool:
    return any(lo <= i <= hi for lo, hi in bands)


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__)
        return 2
    try:
        import yaml
    except ImportError:
        print("check_ids: PyYAML not installed; skipping.", file=sys.stderr)
        return 0

    module_ids = Path(argv[1])
    content_ids = Path(argv[2]) if len(argv) > 2 else None

    if not module_ids.exists():
        print(f"check_ids: {module_ids} does not exist", file=sys.stderr)
        return 1

    pins = yaml.safe_load(module_ids.read_text()) or {}
    content = (yaml.safe_load(content_ids.read_text()) if content_ids and content_ids.exists() else {}) or {}

    errors: list[str] = []
    checked = 0
    for section, bands in SECTION_BANDS.items():
        want = pins.get(section) or {}
        have = content.get(section) or {}
        content_owner = {_entry_id(v): k for k, v in have.items() if _entry_id(v) is not None}
        seen: dict[int, str] = {}
        for key, entry in want.items():
            i = _entry_id(entry)
            if i is None:
                continue
            checked += 1
            if not _in_bands(i, bands):
                errors.append(f"{section}.{key}: id {i} outside allowed bands {bands}")
            if i in seen:
                errors.append(f"{section}: id {i} pinned twice ({seen[i]!r} and {key!r})")
            seen[i] = key
            owner = content_owner.get(i)
            if owner is not None and owner != key:
                errors.append(f"{section}: id {i} for {key!r} already held by {owner!r} in content/ids.yaml")

    if errors:
        for e in errors:
            print(f"ERROR {e}", file=sys.stderr)
        return 1

    print(f"check_ids: OK ({checked} pinned id(s) checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
