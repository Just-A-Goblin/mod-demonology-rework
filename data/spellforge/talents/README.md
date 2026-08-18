SpellForge talent YAML for the 36-node demonology tree lives here.

Each node carries at least: `tier` (1-based), `column`, `ranks`/`points`,
per-rank spell IDs, and `prereq`. `tools/validate_tree.py` asserts the design
shape (nodes per tier `4-3-4-3-4-3-4-4-3-3-1`, points `10-8-8-7-8-8-8-9-6-5-1`,
sum 78) before any build. Full tree authored in Phase 5.
