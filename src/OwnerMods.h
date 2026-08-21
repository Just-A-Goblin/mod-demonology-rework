/*
 * mod-demonology-rework — owner-aura driver (DESIGN_V2 §8.1).
 *
 * The consumer side of OnPoolChanged: recomputes the warlock's own buffs from
 * (talent ranks x commanded-demon count) and applies only the delta, with per-owner
 * bookkeeping so it never drifts and coexists with other buffs. Consumers:
 *   - Overlord's Presence (op): per commanded demon, +owner max health & haste,
 *   - Cursed Vitality (cv): the owner-stamina half (demon half is in DemonHealthMult).
 */
#ifndef MOD_DEMONOLOGY_REWORK_OWNER_MODS_H
#define MOD_DEMONOLOGY_REWORK_OWNER_MODS_H

#include "Define.h"
#include "ObjectGuid.h"

class Player;

namespace Demonology::OwnerMods
{
    // Recompute + apply the owner auras for the given commanded-demon count. Cheap and
    // idempotent (no-ops when nothing changed), so it's safe to call every mirror tick.
    void Apply(Player* owner, uint8 commandedCount);

    // Forget an owner's bookkeeping (call on logout — the unit-mods die with the unit).
    void Clear(ObjectGuid owner);
}

#endif // MOD_DEMONOLOGY_REWORK_OWNER_MODS_H
