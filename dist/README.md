# dist/ — vendored generated artifacts

**Do not hand-edit.** `build.sh` regenerates everything here from
`data/spellforge/**` via SpellForge:

- `dbc/*.dbc` — patched client/server DBCs
- `sql/db-world/*.sql` — spell_dbc / spell_ranks / spell_proc / spell_script_names / …
- `mpq/patch-6.MPQ` — **standalone** client patch (our DBCs only). Useful only on a
  client with no higher-priority patch owning Spell.dbc. On a modded client, use
  `../deploy_client.sh`, which merges our DBCs into the winning patch in place.
- `manifest.json` — sha256 ledger (proves dist/ matches a specific build)

These are committed on purpose: `install.sh` consumes them so the module installs
with no SpellForge present. Empty until the first `build.sh`.
