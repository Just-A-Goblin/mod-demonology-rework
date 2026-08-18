# Core patches

The canonical registry lives next to the patches themselves:
[`../core-patches/CORE_PATCHES.md`](../core-patches/CORE_PATCHES.md).

Patches are applied by `core-patches/apply.sh` and reversed by
`core-patches/revert.sh`, both idempotent. See PLAN §0.1 (fork discipline) and
§3.2 / §8 for the planned seams.
