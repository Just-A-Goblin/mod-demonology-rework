/*
 * mod-demonology-rework — PetScaling (Phase 6).
 *
 * Stat inheritance for the demons we fully control (recruited legionnaires and
 * Wild Imps). These are plain owned creatures, not core Guardians/Pets, so they
 * never touch the core's pet-scaling paths — the core only injects owner spell
 * power for `IsPlayer()` casters. We therefore drive inheritance ourselves:
 *   - melee/health via ApplyInheritance() (cached fields, re-applied on change),
 *   - ability damage live at cast time (see the Wild Imp Firebolt SpellScript),
 * both scaling off the owner's spell power (design §9.4 — warlock demons derive
 * their power from SP, not the owner's attack power).
 */
#ifndef MOD_DEMONOLOGY_REWORK_PET_SCALING_H
#define MOD_DEMONOLOGY_REWORK_PET_SCALING_H

#include "Define.h"
#include "ObjectGuid.h"

class Player;
class Creature;

namespace Demonology::PetScaling
{
    // Owner spell power a demon inherits from. Warlock gear/talents grant
    // school-mask MAGIC spell power, so the shadow-school base captures it.
    int32 OwnerSpellPower(Player* owner);

    // Apply full stat inheritance to one owned demon: health scaled off the owner,
    // melee weapon damage scaled off owner SP. Idempotent — safe to re-run on
    // summon, gear change, or an owner aura change that moved SP.
    void ApplyInheritance(Player* owner, Creature* demon);

    // Re-apply inheritance to every demon the owner controls (pool legionnaires +
    // Wild Imps). Called when the owner's SP may have changed (e.g. gear swap).
    void ReapplyAll(Player* owner);

    // Apply the anchor pet's talent buffs (health/attack-speed as percent modifiers,
    // idempotent). Cheap; safe to call every mirror tick and on talent/gear change.
    void ApplyPetMods(Player* owner);

    // Drop the per-owner pet-mod bookkeeping (call on logout).
    void ForgetPet(ObjectGuid owner);
}

#endif // MOD_DEMONOLOGY_REWORK_PET_SCALING_H
