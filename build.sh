#!/usr/bin/env bash
#
# build.sh — AUTHOR-TIME build (requires SpellForge).
#
# Pipeline: validate module YAML -> link it into the SpellForge content project
# -> `sf build` -> vendor the generated artifacts back into dist/ so the module
# is self-contained and installable offline (see install.sh).
#
# Overridable env:
#   SPELLFORGE_CONTENT  path to the spellforge-content project (default: sibling)
#   SF_BIN              path to the `sf` CLI (default: PATH, then mcp venv)
#
set -euo pipefail

MODULE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SPELLFORGE_CONTENT="${SPELLFORGE_CONTENT:-$(cd "$MODULE_DIR/../spellforge-content" 2>/dev/null && pwd || true)}"

# --- resolve the sf CLI ---
SF_BIN="${SF_BIN:-}"
if [[ -z "$SF_BIN" ]]; then
    if command -v sf >/dev/null 2>&1; then
        SF_BIN="sf"
    elif [[ -x "$MODULE_DIR/../mcp-acore-spellforge/.venv/bin/sf" ]]; then
        SF_BIN="$MODULE_DIR/../mcp-acore-spellforge/.venv/bin/sf"
    else
        echo "ERROR: sf CLI not found. Install SpellForge (mcp-acore-spellforge/install.sh) or set SF_BIN." >&2
        exit 1
    fi
fi
[[ -d "$SPELLFORGE_CONTENT" ]] || { echo "ERROR: spellforge-content not found; set SPELLFORGE_CONTENT." >&2; exit 1; }

echo "==> validating module YAML (before touching SpellForge)"
python3 "$MODULE_DIR/tools/validate_tree.py" "$MODULE_DIR/data/spellforge"
python3 "$MODULE_DIR/tools/check_ids.py"     "$MODULE_DIR/data/spellforge/ids.yaml" "$SPELLFORGE_CONTENT/content/ids.yaml"

# --- stage the module's authoring YAML into the content project ---
# SpellForge's loader scans content/spells/**.yaml and content/talents/**.yaml.
# Its rglob does NOT traverse directory symlinks, so we make a real `demonology`
# subdir and symlink each YAML file into it (files-in-a-real-dir ARE found).
echo "==> staging authoring YAML into $SPELLFORGE_CONTENT/content/{spells,talents}/demonology"
shopt -s nullglob
for kind in spells talents; do
    src="$MODULE_DIR/data/spellforge/$kind"
    dst="$SPELLFORGE_CONTENT/content/$kind/demonology"
    rm -rf "$dst"; mkdir -p "$dst"
    for f in "$src"/*.yaml "$src"/*.yml; do
        ln -s "$f" "$dst/$(basename "$f")"
    done
done

# --- seed our pinned IDs into the shared registry (append-only; errors on conflict) ---
echo "==> seeding pinned IDs into content/ids.yaml"
python3 "$MODULE_DIR/tools/seed_ids.py" "$MODULE_DIR/data/spellforge/ids.yaml" "$SPELLFORGE_CONTENT/content/ids.yaml"

echo "==> sf build"
export SPELLFORGE_PROJECT="$SPELLFORGE_CONTENT"
( cd "$SPELLFORGE_CONTENT" && "$SF_BIN" build --targets dbc,sql,mpq )

# --- vendor artifacts into dist/ (the offline install source) ---
# Copy EXACTLY what this build declared in manifest.json, so dist/ mirrors the
# build precisely — no stale or foreign artifacts (e.g. someone else's MPQ) leak in.
BUILD="$SPELLFORGE_CONTENT/build"
echo "==> vendoring artifacts into dist/ (from manifest.json)"
rm -rf "$MODULE_DIR/dist/dbc" "$MODULE_DIR/dist/sql" "$MODULE_DIR/dist/mpq"
python3 - "$BUILD" "$MODULE_DIR/dist" <<'PY'
import json, shutil, sys
from pathlib import Path
build, dist = Path(sys.argv[1]), Path(sys.argv[2])
man = json.loads((build / "manifest.json").read_text())
arts = man.get("artifacts", {})
for rel in arts:
    dst = dist / rel
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / rel, dst)
shutil.copy2(build / "manifest.json", dist / "manifest.json")
print(f"    vendored {len(arts)} artifact(s) + manifest.json")
PY

echo "==> done. dist/ now holds the installable artifacts. Commit dist/ to archive the module."
