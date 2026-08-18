World-DB rows SpellForge does not manage — shipped by this module and applied by
`install.sh` (and by AzerothCore's auto-updater when the module is linked in).

Planned (Phase 3 / Phase 6):
- `creature_template` for new demon entries `600000–600099`.
- `pet_levelstats` rows for every new entry (else base HP at 80 is nonsense — PLAN §8).

Keep files numbered (`NN_name.sql`) so apply order is deterministic.
