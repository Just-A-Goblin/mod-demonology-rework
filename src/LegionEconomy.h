/*
 * mod-demonology-rework — LegionEconomy: the narrow, allowlisted entry points by which NON-demon
 * sources may feed the two shard/brand economies. Fel Corruption (rc redesign, Addendum A.2) routes
 * Corruption ticks through QualifyPlayerPeriodic so they feed EXACTLY Soul Harvest + Doombrand and
 * nothing else — never the demon-damage multiplier stack (the fb/290515-class-of-bug prevention).
 */
#ifndef MOD_DEMONOLOGY_REWORK_LEGION_ECONOMY_H
#define MOD_DEMONOLOGY_REWORK_LEGION_ECONOMY_H

#include "Define.h"

class Player;
class Unit;

namespace Demonology::LegionEconomy
{
    // Grant a Soul Shard if chancePct (0..100) rolls AND the per-player Soul Harvest ICD has
    // elapsed. Shared by the demon-damage hook and Fel Corruption. Returns true on grant.
    bool TrySoulHarvestGrant(Player* owner, float chancePct, uint32 icdMs);

    // A PLAYER periodic tick (Fel Corruption's Corruption) qualifies for Soul Harvest + Doombrand
    // at effectiveness `eff` (0..1). Feeds ONLY those two — no bt/gb/fcd/pf, no demon mult stack.
    void QualifyPlayerPeriodic(Player* owner, Unit* target, float dmg, float eff);
}

#endif // MOD_DEMONOLOGY_REWORK_LEGION_ECONOMY_H
