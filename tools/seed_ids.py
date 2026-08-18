#!/usr/bin/env python3
"""
seed_ids.py — merge the module's pinned IDs into SpellForge's content/ids.yaml.

SpellForge allocates IDs by key (append-only) and respects pre-existing pins, so
seeding our keys at their assigned IDs keeps the demon content in its documented
290000 blocks instead of being auto-allocated from the bottom of the range.

Usage:
    seed_ids.py <module ids.yaml> <content/ids.yaml>

Idempotent and safe:
- key absent in content    -> added with our pinned id
- key present, same id      -> left alone
- key present, different id -> ERROR (never silently move an allocated id)
- our id already held by a different key -> ERROR (collision)
"""
from __future__ import annotations

import sys
from pathlib import Path

SECTIONS = ("spells", "talents", "talent_tabs", "spell_icons", "glyph_properties",
            "skill_line_abilities")


def _entry_id(entry):
    return entry if isinstance(entry, int) else (entry or {}).get("id")


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__)
        return 2
    try:
        import yaml
    except ImportError:
        print("seed_ids: PyYAML required", file=sys.stderr)
        return 1

    module_ids = Path(argv[1])
    content_ids = Path(argv[2])

    pins = yaml.safe_load(module_ids.read_text()) or {}
    content = yaml.safe_load(content_ids.read_text()) if content_ids.exists() else {}
    content = content or {}

    errors: list[str] = []
    added = 0
    for section in SECTIONS:
        want = pins.get(section) or {}
        have = content.setdefault(section, {}) or {}
        content[section] = have
        # index existing ids -> key for collision detection
        id_owner = {_entry_id(v): k for k, v in have.items() if _entry_id(v) is not None}
        for key, entry in want.items():
            new_id = _entry_id(entry)
            if new_id is None:
                continue
            cur = have.get(key)
            if cur is not None:
                if _entry_id(cur) != new_id:
                    errors.append(f"{section}.{key}: pinned {new_id} but content has {_entry_id(cur)}")
                continue
            owner = id_owner.get(new_id)
            if owner is not None and owner != key:
                errors.append(f"{section}: id {new_id} for {key!r} already held by {owner!r}")
                continue
            have[key] = new_id
            id_owner[new_id] = key
            added += 1

    if errors:
        for e in errors:
            print(f"ERROR {e}", file=sys.stderr)
        return 1

    if added:
        content_ids.write_text(yaml.safe_dump(content, sort_keys=True, default_flow_style=False))
    print(f"seed_ids: OK ({added} pin(s) added).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
