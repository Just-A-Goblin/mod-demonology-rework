# ID ranges

**Audit result (Phase 0 Task 1): the custom ID space is empty.** The shared
registry `spellforge-content/content/ids.yaml` holds only a retired
`druid.ice_lance` (200000) and a stale `mage.test_bolt` example. All assignments
below sit inside SpellForge's configured ranges (`spellforge-content/spellforge.toml`
`[ranges]`). `tools/check_ids.py` enforces them at build time.

| Asset | Assigned block | SpellForge range | Notes |
|---|---|---|---|
| Player-facing spells | `290000–290499` | spell `200000–299999` | |
| Internal/hidden spells | `290500–290899` | spell `200000–299999` | pool tags, threat auras, ICD markers |
| Pet ability spells | `290900–291199` | spell `200000–299999` | Firebolt, Doom Bolt, etc. |
| Talent entries | `9000–9099` | talent `5000–9999` | |
| Talent **tab** | *reuse existing Demonology tab* | (talent_tab `500–999` = new tabs) | replace in place; read real id from `TalentTab.dbc` |
| Spell icons (custom) | `19000–19099` | spell_icon `10000–19999` | only if hand-authored icons |
| Creature entries | `600000–600099` | *(world DB — not SpellForge)* | ships in `data/sql/db-world/` |
| Soul Shard reagent | `6265` (existing) | — | no new item |

Dropped from the original plan: the out-of-range `300000`/`3000` proposals.
