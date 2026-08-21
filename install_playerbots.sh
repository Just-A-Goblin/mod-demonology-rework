#!/usr/bin/env bash
#
# install_playerbots.sh — install the Demonology-rework integration into mod-playerbots.
#
# Two independent things get deployed (both idempotent):
#   1. the C++ patch  playerbots-patches/0001-demonology-integration.patch  -> the mod-playerbots
#      git checkout (verified with `git apply --check`; refuses if the tree has drifted from the pin)
#   2. the vendored conf fragment  conf/playerbots-demonology.conf.fragment  -> merged into the live
#      playerbots.conf inside a managed  # BEGIN/END mod-demonology-rework  marker block
#
# This script NEVER compiles and NEVER restarts — it prints the rebuild/restart reminder instead.
#
# Usage:
#   ./install_playerbots.sh            # DRY-RUN (default): report what --apply would do
#   ./install_playerbots.sh --apply    # apply the patch + merge the conf block
#   ./install_playerbots.sh --revert   # reverse the patch + remove the conf block (restore pristine)
#
# Overridable env (defaults match this machine):
#   AC_SOURCE, WORLDSERVER_CONF, PB_DIR
#
set -euo pipefail

MODULE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

AC_SOURCE="${AC_SOURCE:-/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore}"
WORLDSERVER_CONF="${WORLDSERVER_CONF:-/home/leo/WOW-BACKUP-8-11-26/wow/server/etc/worldserver.conf}"
PB_DIR="${PB_DIR:-$AC_SOURCE/modules/mod-playerbots}"

PIN="085e127e38bcc9952338e40e45a8a22472585502"
PATCH="$MODULE_DIR/playerbots-patches/0001-demonology-integration.patch"
FRAGMENT="$MODULE_DIR/conf/playerbots-demonology.conf.fragment"
LIVE_CONF="$(dirname "$WORLDSERVER_CONF")/modules/playerbots.conf"
BEGIN_RE='^# BEGIN mod-demonology-rework'
END_RE='^# END mod-demonology-rework'

MODE="dryrun"
for arg in "$@"; do
    case "$arg" in
        --apply)  MODE="apply" ;;
        --revert) MODE="revert" ;;
        --dry-run) MODE="dryrun" ;;
        *) echo "unknown arg: $arg" >&2; exit 2 ;;
    esac
done
run() { if [[ "$MODE" != "dryrun" ]]; then eval "$@"; else echo "  DRY-RUN: $*"; fi; }
die() { echo "ERROR: $*" >&2; exit 1; }

echo "== mod-demonology-rework -> mod-playerbots  ($(echo "$MODE" | tr a-z A-Z)) =="

# --- preflight -------------------------------------------------------------
[[ -f "$PATCH" ]]    || die "patch not found: $PATCH"
[[ -f "$FRAGMENT" ]] || die "conf fragment not found: $FRAGMENT (run tools/gen_playerbot_spec_links.py)"
[[ -d "$PB_DIR/.git" ]] || die "mod-playerbots is not a git checkout: $PB_DIR"

HEAD="$(git -C "$PB_DIR" rev-parse HEAD 2>/dev/null || echo unknown)"
if [[ "$HEAD" != "$PIN" ]]; then
    echo "  WARNING: mod-playerbots HEAD is $HEAD"
    echo "           patch was generated against pin $PIN — apply may fail if the tree diverged."
fi

# patch state: applied? reverse-check succeeds iff the changes are already present.
patch_applied()   { git -C "$PB_DIR" apply --reverse --check "$PATCH" >/dev/null 2>&1; }
patch_appliable() { git -C "$PB_DIR" apply --check "$PATCH" >/dev/null 2>&1; }

# strip the managed conf block from a file (inclusive of the BEGIN/END lines).
strip_block() { awk -v b="$BEGIN_RE" -v e="$END_RE" '$0 ~ b {s=1} !s {print} $0 ~ e {s=0}' "$1"; }

OVERRIDE_MARK="#DEMONOLOGY-REWORK-OVERRIDDEN# "

# Build the live conf's new contents on stdout: the existing conf with (a) our managed block removed
# and (b) any pre-existing line whose KEY we own commented out — because AzerothCore's config is
# FIRST-WINS on duplicate keys, so a stock PremadeSpecLink.9.1.* would otherwise shadow ours. Then
# our fragment (which carries its own BEGIN/END markers) is appended. Only keys the fragment actually
# defines are touched, so affliction/destruction/other-level links are left alone. Reversible via the
# marker prefix.
merged_conf() {
    local conf="$1"
    local keys; keys="$(grep -oE '^AiPlayerbot\.[^ =]+' "$FRAGMENT" | sort -u)"
    strip_block "$conf" | awk -v mark="$OVERRIDE_MARK" -v keylist="$keys" '
        BEGIN { n = split(keylist, a, "\n"); for (i = 1; i <= n; i++) own[a[i]] = 1 }
        {
            key = $0; sub(/[ \t]*=.*/, "", key); gsub(/[ \t]/, "", key)
            if ((key in own) && $0 !~ /^#/) print mark $0
            else print $0
        }'
    cat "$FRAGMENT"   # carries its own BEGIN/END markers; no extra separator so revert is byte-exact
}

# Reverse of merged_conf: drop our managed block and un-comment the lines we overrode.
unmerged_conf() {
    strip_block "$1" | sed "s/^${OVERRIDE_MARK}//"
}

reminder() {
    echo
    echo "  NEXT (this script does not compile or restart):"
    echo "    1. rebuild:  cd /home/leo/wow/build && make -j\$(nproc) worldserver && make install"
    echo "    2. restart the worldserver (conf + C++ both need a fresh boot)"
    echo "    3. single-bot dry run BEFORE fleet exposure — see tools/playerbot_talent_check.sql"
}

case "$MODE" in
apply|dryrun)
    # 1. C++ patch --------------------------------------------------------
    echo "[1/2] C++ patch -> $PB_DIR"
    if patch_applied; then
        echo "  already applied — no-op (idempotent)"
    elif patch_appliable; then
        run "git -C '$PB_DIR' apply '$PATCH'"
        echo "  applied 0001-demonology-integration.patch"
    else
        die "patch does not apply cleanly — checkout drifted from pin $PIN. Refusing (fix/revert first)."
    fi

    # 2. conf block -------------------------------------------------------
    echo "[2/2] conf block -> $LIVE_CONF"
    if [[ ! -f "$LIVE_CONF" ]]; then
        echo "  WARNING: live playerbots.conf not found; skipping conf merge ($LIVE_CONF)"
    elif [[ "$MODE" == "dryrun" ]]; then
        owned="$(grep -oE '^AiPlayerbot\.[^ =]+' "$FRAGMENT" | sort -u \
            | while read -r k; do grep -qE "^${k//./\\.}[ =]" "$LIVE_CONF" && echo "$k"; done | wc -l)"
        echo "  DRY-RUN: append managed block ($(wc -l < "$FRAGMENT") lines); comment out $owned superseded stock key(s)"
    else
        cp "$LIVE_CONF" "$LIVE_CONF.bak-$(date +%Y%m%d-%H%M%S)"
        tmp="$(mktemp)"
        merged_conf "$LIVE_CONF" > "$tmp"
        mv "$tmp" "$LIVE_CONF"
        echo "  merged managed block + commented superseded stock keys (backup alongside as .bak-*)"
    fi
    reminder
    ;;

revert)
    echo "[1/2] reverse C++ patch"
    if patch_applied; then
        run "git -C '$PB_DIR' apply --reverse '$PATCH'"
        echo "  reverted (tree restored to pristine)"
    else
        echo "  patch not currently applied — nothing to reverse"
    fi
    echo "[2/2] remove conf block"
    if [[ -f "$LIVE_CONF" ]] && grep -qE "$BEGIN_RE" "$LIVE_CONF"; then
        cp "$LIVE_CONF" "$LIVE_CONF.bak-$(date +%Y%m%d-%H%M%S)"
        tmp="$(mktemp)"; unmerged_conf "$LIVE_CONF" > "$tmp"; mv "$tmp" "$LIVE_CONF"
        echo "  removed managed block + restored superseded stock keys"
    else
        echo "  no managed block present — nothing to remove"
    fi
    reminder
    ;;
esac

echo "== done =="
