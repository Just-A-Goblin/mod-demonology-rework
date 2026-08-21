/*
 * mod-demonology-rework — Fervent Standard (fs, Addendum A.3) shared range/multiplier helpers.
 *
 * The buff *is* three range checks against the owner's Demonic Circle GameObject (48018) — no
 * aura bookkeeping, no position cache. We resolve the circle via Player::GetGameObject(), the
 * same cheap owner-owned-object lookup Riftwalker already uses (not a world search), so expiry,
 * re-summon and map change are handled for free by the core.
 *
 * Qualification (Addendum A.5): the DEMON offense bonus is an ordinary demon-damage multiplier
 * (flows into SH/brand/bt naturally, like fa/vp). The OWNER offense multiplies player damage.
 * The mitigation is applied on the demon damage-TAKEN side (both melee and spell — the fb bug
 * was a spell-only check; don't repeat it). All three return the identity value when untalented
 * or out of radius, so callers can multiply unconditionally.
 */
#ifndef MOD_DEMONOLOGY_FERVENT_STANDARD_H
#define MOD_DEMONOLOGY_FERVENT_STANDARD_H

#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "GameObject.h"
#include "Player.h"
#include "Unit.h"

namespace Demonology
{
    // Is `u` standing within the owner's planted Demonic Circle radius (same map)? False if the
    // owner has no circle up. Cheap: GetGameObject walks the owner's tiny owned-object list.
    inline bool InFerventRadius(Player* owner, Unit const* u)
    {
        if (!owner || !u)
            return false;
        GameObject* circle = owner->GetGameObject(SPELL_DEMONIC_CIRCLE_SUMMON);
        if (!circle || u->GetMapId() != circle->GetMapId())
            return false;
        return u->IsWithinDist3d(circle, gConfig.FerventStandardRadius);
    }

    // Offense multiplier for a unit hitting inside the standard: owner OR a commanded demon.
    // Pass the attacking unit; for the owner's own hits pass the owner itself.
    inline float FerventStandardOffenseMult(Player* owner, Unit const* attacker)
    {
        float const pct = FerventStandardDamagePct(owner);
        if (pct <= 0.0f || !InFerventRadius(owner, attacker))
            return 1.0f;
        return 1.0f + pct;
    }

    // Damage-taken multiplier for a commanded demon standing inside the standard (demons only —
    // the warlock's own defense is the legion, per design).
    inline float FerventStandardMitigationMult(Player* owner, Unit const* demon)
    {
        float const pct = FerventStandardMitigationPct(owner);
        if (pct <= 0.0f || !InFerventRadius(owner, demon))
            return 1.0f;
        return 1.0f - pct;
    }
}

#endif // MOD_DEMONOLOGY_FERVENT_STANDARD_H
