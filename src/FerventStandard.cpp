/*
 * mod-demonology-rework — Fervent Standard (fs, Addendum A.3) cosmetic icon ticker.
 *
 * ALL of fs's math (owner/demon offense, demon mitigation) is direct range checks in
 * FerventStandard.h, applied inside the demon-damage hook (ShardEconomy.cpp). This file is pure UI
 * polish: a ~1s ticker that keeps a zero-effect buff icon (290517) on the warlock AND each commanded
 * demon while that unit stands inside the warlock's Demonic Circle radius, so the banner's reach is
 * visible on everyone it affects. No gameplay depends on it.
 *
 * The icon aura is INFINITE-duration (no countdown) and applied ONCE per unit on entering the radius,
 * removed ONCE on leaving — never re-applied while present, so it never flickers/refreshes and shows
 * no cast animation (the visual is also stripped in content). Wild Imps are intentionally skipped
 * (too transient to bother iconizing; they still get the damage bonus via the range check).
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "FerventStandard.h"

#include "Creature.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <unordered_map>

using namespace Demonology;

namespace
{
    // Bring `u`'s icon state in line with whether it should have the banner right now. State-change
    // only: apply once when it should have it and doesn't, remove once when it shouldn't and does.
    void SyncIcon(Player* owner, Unit* u, bool want)
    {
        if (!u)
            return;
        bool const has = u->HasAura(SPELL_FERVENT_STANDARD_ICON);
        if (want && !has)
            owner->AddAura(SPELL_FERVENT_STANDARD_ICON, u);         // caster = owner; applies on u, no cast/visual
        else if (!want && has)
            u->RemoveAurasDueToSpell(SPELL_FERVENT_STANDARD_ICON);
    }
}

class demonology_fervent_standard : public PlayerScript
{
public:
    demonology_fervent_standard() : PlayerScript("demonology_fervent_standard") { }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!gConfig.Enable || !player || player->getClass() != CLASS_WARLOCK)
            return;

        uint32& acc = _tick[player->GetGUID()];
        acc += diff;
        if (acc < 1000)
            return;
        acc = 0;

        // Untalented (or after a respec) → want is false everywhere, so this also cleans up any
        // lingering icons on the owner and demons.
        bool const talented = Demonology::FerventStandardDamagePct(player) > 0.0f;

        SyncIcon(player, player, talented && Demonology::InFerventRadius(player, player));

        if (Pet* pet = player->GetPet())
            SyncIcon(player, pet, talented && Demonology::InFerventRadius(player, pet));

        if (CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
        {
            for (ObjectGuid g : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*player, g))
                    SyncIcon(player, c, talented && Demonology::InFerventRadius(player, c));
            if (Creature* gd = ObjectAccessor::GetCreature(*player, pool->GreaterDemonGuid()))
                SyncIcon(player, gd, talented && Demonology::InFerventRadius(player, gd));
        }
    }

private:
    std::unordered_map<ObjectGuid, uint32> _tick;
};

void AddSC_demonology_fervent_standard()
{
    new demonology_fervent_standard();
}
