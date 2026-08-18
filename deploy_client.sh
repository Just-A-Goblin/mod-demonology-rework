#!/usr/bin/env bash
#
# deploy_client.sh — install the client DBC patch (needs SpellForge/StormLib).
#
# Why this is separate from install.sh (which is offline): on a modded 3.3.5
# client, a higher-priority patch (letters load after numbers) may already supply
# Spell.dbc / SkillLineAbility.dbc. WoW would then IGNORE a fresh standalone
# patch. So our DBCs must be MERGED into the winning patch in place, preserving
# that patch's other files. That merge reads the target client's MPQ and repacks
# it — it cannot be pre-baked into dist/ for an arbitrary client.
#
# Requires a prior ./build.sh (SpellForge build/ must be current).
# Dry-run by default; pass --apply to copy into the client.
#
#   CLIENT_DATA=/path/to/WoW/Data ./deploy_client.sh --apply
#
set -euo pipefail

MODULE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SPELLFORGE_CONTENT="${SPELLFORGE_CONTENT:-$(cd "$MODULE_DIR/../spellforge-content" 2>/dev/null && pwd || true)}"
CLIENT_DATA="${CLIENT_DATA:-/mnt/c/GAMES/WoW-3.3.5a/Data}"

SF_BIN="${SF_BIN:-}"
if [[ -z "$SF_BIN" ]]; then
    if command -v sf >/dev/null 2>&1; then SF_BIN="sf"
    elif [[ -x "$MODULE_DIR/../mcp-acore-spellforge/.venv/bin/sf" ]]; then SF_BIN="$MODULE_DIR/../mcp-acore-spellforge/.venv/bin/sf"
    else echo "ERROR: sf CLI not found — the client merge needs SpellForge/StormLib." >&2; exit 1; fi
fi
export SPELLFORGE_PROJECT="$SPELLFORGE_CONTENT"

APPLY=0; [[ "${1:-}" == "--apply" ]] && APPLY=1

[[ -f "$SPELLFORGE_CONTENT/spellforge.toml" ]] || { echo "ERROR: no spellforge.toml in $SPELLFORGE_CONTENT" >&2; exit 1; }

echo "== client patch diagnosis =="
( cd "$SPELLFORGE_CONTENT" && "$SF_BIN" patch doctor ) || true

echo ; echo "== building merged client patch (writes to build/, not the client) =="
merge_log="$(cd "$SPELLFORGE_CONTENT" && "$SF_BIN" patch merge ${INTO:+--into "$INTO"} 2>&1)"
printf '%s\n' "$merge_log"
merged="$(printf '%s\n' "$merge_log" | sed -n 's/.*built \(\/[^ ]*\.[Mm][Pp][Qq]\).*/\1/p' | head -1)"
[[ -n "$merged" && -f "$merged" ]] || { echo "ERROR: could not locate the merged patch from 'sf patch merge' output." >&2; exit 1; }

name="$(basename "$merged")"
dst="$CLIENT_DATA/$name"
echo ; echo "== deploy: $merged -> $dst =="
if [[ $APPLY -eq 1 ]]; then
    if [[ -f "$dst" ]]; then
        bak="$MODULE_DIR/.install-backups/client-$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$bak"; cp -p "$dst" "$bak/"; echo "backed up existing $name -> $bak/"
    fi
    cp -f "$merged" "$dst"
    echo "copied. Restart the client so it reloads $name."
else
    echo "DRY-RUN: cp -f '$merged' '$dst'   (re-run with --apply)"
fi
