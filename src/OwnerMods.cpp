/*
 * mod-demonology-rework — owner-aura driver implementation (DESIGN_V2 §8.1).
 */
#include "OwnerMods.h"

#include "DemonologyTalents.h"

#include "Player.h"
#include "Unit.h"

#include <cmath>
#include <unordered_map>

namespace Demonology::OwnerMods
{
    namespace
    {
        // What we currently have applied to each owner (fractions), so re-apply moves the
        // delta only.
        struct Applied { float health = 0.0f; float haste = 0.0f; float stamina = 0.0f; };
        std::unordered_map<ObjectGuid, Applied> g_mods;

        // Move a TOTAL_PCT stat modifier from fraction cur -> want (ratio delta, like the
        // pet-mod bookkeeping — never drifts, preserves other buffs).
        void MoveStatPct(Player* owner, UnitMods mod, float curFrac, float wantFrac)
        {
            if (std::fabs(wantFrac - curFrac) < 0.0001f)
                return;
            float const curM = 1.0f + curFrac;
            float const wantM = 1.0f + wantFrac;
            owner->ApplyStatPctModifier(mod, TOTAL_PCT, (wantM / curM - 1.0f) * 100.0f);
        }

        // Move player haste (cast + all swing timers) from fraction cur -> want. The
        // percent mods are exactly reversible, so we remove the old amount then add the new.
        void MoveHaste(Player* owner, float curFrac, float wantFrac)
        {
            if (std::fabs(wantFrac - curFrac) < 0.0001f)
                return;
            float const curPct = curFrac * 100.0f;
            float const wantPct = wantFrac * 100.0f;
            if (curPct > 0.0f)
            {
                owner->ApplyCastTimePercentMod(curPct, false);
                owner->ApplyAttackTimePercentMod(BASE_ATTACK, curPct, false);
                owner->ApplyAttackTimePercentMod(OFF_ATTACK, curPct, false);
                owner->ApplyAttackTimePercentMod(RANGED_ATTACK, curPct, false);
            }
            if (wantPct > 0.0f)
            {
                owner->ApplyCastTimePercentMod(wantPct, true);
                owner->ApplyAttackTimePercentMod(BASE_ATTACK, wantPct, true);
                owner->ApplyAttackTimePercentMod(OFF_ATTACK, wantPct, true);
                owner->ApplyAttackTimePercentMod(RANGED_ATTACK, wantPct, true);
            }
        }
    }

    void Apply(Player* owner, uint8 commandedCount)
    {
        if (!owner)
            return;

        float const wantHealth  = OverlordsPresenceHealthPerDemon(owner) * float(commandedCount);
        float const wantHaste   = OverlordsPresenceHastePerDemon(owner) * float(commandedCount);
        float const wantStamina = CursedVitalityOwnerStamina(owner);

        Applied& cur = g_mods[owner->GetGUID()];
        MoveStatPct(owner, UNIT_MOD_HEALTH,       cur.health,  wantHealth);
        MoveStatPct(owner, UNIT_MOD_STAT_STAMINA, cur.stamina, wantStamina);
        MoveHaste(owner, cur.haste, wantHaste);
        cur.health  = wantHealth;
        cur.stamina = wantStamina;
        cur.haste   = wantHaste;
    }

    void Clear(ObjectGuid owner)
    {
        g_mods.erase(owner);
    }
}
