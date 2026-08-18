#!/usr/bin/env bash
#
# apply.sh — apply this module's core patches to the AzerothCore source.
#
# Idempotent: skips a patch that is already applied (git apply -R --check).
# Fails loudly if a patch neither applies nor is already applied — that is the
# signal an upstream merge shifted the code out from under us (PLAN §0.1).
#
#   AC_SOURCE=/path/to/azerothcore bash apply.sh
#
set -euo pipefail

PATCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
AC_SOURCE="${AC_SOURCE:-/home/leo/WOW-BACKUP-8-11-26/wow/azerothcore}"
[[ -d "$AC_SOURCE/.git" ]] || { echo "ERROR: AC_SOURCE ($AC_SOURCE) is not a git repo." >&2; exit 1; }

shopt -s nullglob
patches=("$PATCH_DIR"/*.patch)
if [[ ${#patches[@]} -eq 0 ]]; then
    echo "apply.sh: no *.patch files yet — nothing to do."
    exit 0
fi

cd "$AC_SOURCE"
for p in "${patches[@]}"; do
    name="$(basename "$p")"
    if git apply -R --check "$p" >/dev/null 2>&1; then
        echo "  = $name already applied — skipping"
    elif git apply --check "$p" >/dev/null 2>&1; then
        git apply "$p"
        echo "  + $name applied"
    else
        echo "ERROR: $name will not apply cleanly and is not already applied." >&2
        echo "       Upstream likely moved. Regenerate the patch (see CORE_PATCHES.md)." >&2
        exit 1
    fi
done
echo "apply.sh: done. Rebuild the worldserver."
