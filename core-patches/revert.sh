#!/usr/bin/env bash
#
# revert.sh — reverse this module's core patches (idempotent).
#
#   AC_SOURCE=/path/to/azerothcore bash revert.sh
#
set -euo pipefail

PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_SOURCE="${AC_SOURCE:-/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore}"
[[ -d "$AC_SOURCE/.git" ]] || { echo "ERROR: AC_SOURCE ($AC_SOURCE) is not a git repo." >&2; exit 1; }

shopt -s nullglob
patches=("$PATCH_DIR"/*.patch)
[[ ${#patches[@]} -eq 0 ]] && { echo "revert.sh: no *.patch files — nothing to do."; exit 0; }

cd "$AC_SOURCE"
# reverse order
for (( i=${#patches[@]}-1; i>=0; i-- )); do
    p="${patches[$i]}"; name="$(basename "$p")"
    if git apply -R --check "$p" >/dev/null 2>&1; then
        git apply -R "$p"
        echo "  - $name reverted"
    else
        echo "  = $name not applied — skipping"
    fi
done
echo "revert.sh: done. Rebuild the worldserver."
