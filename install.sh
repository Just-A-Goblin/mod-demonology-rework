#!/usr/bin/env bash
#
# install.sh — OFFLINE install from vendored artifacts (SpellForge NOT required).
#
# Does everything needed to deploy the module:
#   1. apply core patches to the AzerothCore source
#   2. link the module into AzerothCore's modules/ (rebuild worldserver after!)
#   3. deploy DBCs      -> server data/dbc     (backed up first)
#   4. deploy patch MPQ -> client Data/        (backed up first)
#   5. deploy addons    -> client Interface/AddOns/ (backed up first)
#   6. apply SQL        -> world + characters DBs (creds from worldserver.conf)
#
# Dry-run by default. Pass --apply to execute.
#
# Overridable env (defaults match this machine):
#   AC_SOURCE, SERVER_DBC, CLIENT_DATA, CLIENT_ADDONS, WORLDSERVER_CONF
#
set -euo pipefail

MODULE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODULE_NAME="mod-demonology-rework"

AC_SOURCE="${AC_SOURCE:-/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore}"
SERVER_DBC="${SERVER_DBC:-/home/leo/WOW-BACKUP-8-11-26/wow/server/data/dbc}"
CLIENT_DATA="${CLIENT_DATA:-/mnt/c/GAMES/WoW-3.3.5a/Data}"
CLIENT_ADDONS="${CLIENT_ADDONS:-$(dirname "$CLIENT_DATA")/Interface/AddOns}"
WORLDSERVER_CONF="${WORLDSERVER_CONF:-/home/leo/WOW-BACKUP-8-11-26/wow/server/etc/worldserver.conf}"

APPLY=0
SERVER_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --apply) APPLY=1 ;;
        --server-only) SERVER_ONLY=1 ;;
    esac
done
run() { if [[ $APPLY -eq 1 ]]; then eval "$@"; else echo "  DRY-RUN: $*"; fi; }

TS="$(date +%Y%m%d-%H%M%S)"
BACKUP_DIR="$MODULE_DIR/.install-backups/$TS"
[[ $APPLY -eq 1 ]] && mkdir -p "$BACKUP_DIR"

echo "== mod-demonology-rework install ($([[ $APPLY -eq 1 ]] && echo APPLY || echo DRY-RUN)) =="

# 1. core patches -----------------------------------------------------------
echo "[1/6] core patches"
if [[ $APPLY -eq 1 ]]; then
    AC_SOURCE="$AC_SOURCE" bash "$MODULE_DIR/core-patches/apply.sh"
else
    echo "  DRY-RUN: core-patches/apply.sh against $AC_SOURCE"
fi

# 2. link module ------------------------------------------------------------
echo "[2/6] link module into AzerothCore"
run "ln -sfn '$MODULE_DIR' '$AC_SOURCE/modules/$MODULE_NAME'"
echo "  NOTE: rebuild the worldserver after linking (new C++ sources)."

# 2b. Live module .conf -----------------------------------------------------
# AzerothCore loads mod_demonology_rework.conf (NOT the .conf.dist template — the
# .dist is only a reference), so without a live .conf the server runs entirely on
# code defaults and spams "Missing property" for every key. Seed the live .conf
# from the repo .dist ONCE; never clobber an admin's edits. (New keys added to the
# .dist in later phases fall back to code defaults until re-seeded — benign.)
echo "[2b] ensure live module .conf"
MODULE_CONF_DIR="$(dirname "$WORLDSERVER_CONF")/modules"
LIVE_CONF="$MODULE_CONF_DIR/mod_demonology_rework.conf"
DIST_SRC="$MODULE_DIR/conf/mod_demonology_rework.conf.dist"
if [[ -f "$LIVE_CONF" ]]; then
    echo "  live .conf exists — left untouched ($LIVE_CONF)"
elif [[ -f "$DIST_SRC" ]]; then
    run "mkdir -p '$MODULE_CONF_DIR'"
    run "cp '$DIST_SRC' '$LIVE_CONF'"
    echo "  seeded live .conf from repo .dist -> $LIVE_CONF"
else
    echo "  WARN: repo .dist missing at $DIST_SRC"
fi

# 3. DBCs -> server ---------------------------------------------------------
echo "[3/6] deploy DBCs -> $SERVER_DBC"
if compgen -G "$MODULE_DIR/dist/dbc/*.dbc" >/dev/null; then
    for f in "$MODULE_DIR"/dist/dbc/*.dbc; do
        dst="$SERVER_DBC/$(basename "$f")"
        [[ -f "$dst" ]] && run "cp -p '$dst' '$BACKUP_DIR/'"
        run "cp -f '$f' '$dst'"
    done
else
    echo "  (no vendored DBCs — run build.sh first)"
fi

# 4. Client DBC patch -------------------------------------------------------
# NOT a plain copy: on a modded client a higher-priority patch may already own
# Spell.dbc, so our DBCs must be MERGED into the winning patch (see deploy_client.sh).
# That merge needs SpellForge/StormLib, so it lives outside this offline script.
echo "[4/6] client DBC patch"
if [[ $SERVER_ONLY -eq 1 ]]; then
    echo "  --server-only: skipping client patch (run ./deploy_client.sh separately)."
elif [[ -x "$MODULE_DIR/deploy_client.sh" ]] && { command -v sf >/dev/null 2>&1 || [[ -x "$MODULE_DIR/../mcp-acore-spellforge/.venv/bin/sf" ]]; }; then
    if [[ $APPLY -eq 1 ]]; then
        CLIENT_DATA="$CLIENT_DATA" bash "$MODULE_DIR/deploy_client.sh" --apply
    else
        CLIENT_DATA="$CLIENT_DATA" bash "$MODULE_DIR/deploy_client.sh"
    fi
else
    echo "  SpellForge not found — run the client patch step separately with SpellForge available:"
    echo "    CLIENT_DATA='$CLIENT_DATA' ./deploy_client.sh --apply"
    echo "  (Our DBCs are in dist/dbc/. A plain patch copy is ignored if a letter patch already owns Spell.dbc.)"
fi

# 5. Client addons ----------------------------------------------------------
# Copy each addon/<Name>/ into the client's Interface/AddOns/. Client-side, so
# --server-only skips it. New addons must be enabled at the character screen.
echo "[5/6] deploy client addons -> $CLIENT_ADDONS"
if [[ $SERVER_ONLY -eq 1 ]]; then
    echo "  --server-only: skipping client addons."
elif compgen -G "$MODULE_DIR/addon/*/" >/dev/null; then
    run "mkdir -p '$CLIENT_ADDONS'"
    for a in "$MODULE_DIR"/addon/*/; do
        a="${a%/}"
        name="$(basename "$a")"
        dst="$CLIENT_ADDONS/$name"
        if [[ -d "$dst" ]]; then
            run "cp -a '$dst' '$BACKUP_DIR/addon-$name'"
            run "rm -rf '$dst'"
        fi
        run "cp -a '$a' '$dst'"
        echo "  $name -> $dst"
    done
    echo "  NOTE: enable new addons at the character screen (AddOns button)."
else
    echo "  (no addons in $MODULE_DIR/addon/)"
fi

# 6. SQL -> DBs -------------------------------------------------------------
echo "[6/6] apply SQL"
# Parse "host;port;user;pass;db" from worldserver.conf.
db_field() { grep -E "^\s*$1\s*=" "$WORLDSERVER_CONF" | head -1 | sed -E 's/[^"]*"([^"]*)".*/\1/'; }
apply_sql_dir() { # $1 = conf key, $2..= dirs
    local key="$1"; shift
    local info; info="$(db_field "$key")"
    IFS=';' read -r H P U PW DB <<<"$info"
    for d in "$@"; do
        compgen -G "$d/*.sql" >/dev/null || continue
        for f in "$d"/*.sql; do
            run "mysql -h'$H' -P'$P' -u'$U' -p'$PW' '$DB' < '$f'"
        done
    done
}
apply_sql_dir WorldDatabaseInfo     "$MODULE_DIR/dist/sql/db-world"        "$MODULE_DIR/data/sql/db-world/base"
apply_sql_dir CharacterDatabaseInfo "$MODULE_DIR/dist/sql/db-characters"   "$MODULE_DIR/data/sql/db-characters/base"

echo "== done.$([[ $APPLY -eq 0 ]] && echo '  (dry-run — re-run with --apply)') =="
[[ $APPLY -eq 1 ]] && echo "backups: $BACKUP_DIR"
echo "Reminder: restart worldserver (DBC + SQL changes) and ensure the client loads the MPQ."
