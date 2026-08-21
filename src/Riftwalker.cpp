/*
 * mod-demonology-rework — Riftwalker (rw, DESIGN_V2 §4/§12.2).
 *
 * When the warlock uses Demonic Circle: Teleport (48020), warp every commanded demon (anchor
 * pet + legionnaires + active greater demon) to the demonic circle and give them a short move-
 * speed burst so the legion arrives with you instead of running across the map. Rides the
 * baseline spell — we only watch the cast via PlayerScript and reposition the demons to the
 * circle's position (which is exactly where the teleport sends the player, so timing is moot).
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "GameObject.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "Util.h"

using namespace Demonology;

namespace
{
    // SPELL_DEMONIC_CIRCLE_SUMMON (48018) now lives in DemonologyIds.h (shared with Fervent Standard).
    constexpr uint32 SPELL_DEMONIC_CIRCLE_TELEPORT = 48020;

    void RiftwalkDemon(Creature* demon, float x, float y, float z, float o)
    {
        if (!demon || !demon->IsAlive())
            return;
        // Small spread so the legion doesn't stack on one point; the AI re-forms next tick.
        float const sx = x + frand(-2.0f, 2.0f);
        float const sy = y + frand(-2.0f, 2.0f);
        demon->NearTeleportTo(sx, sy, z, o);
        demon->CastCustomSpell(SPELL_RIFTWALKER_HASTE, SPELLVALUE_BASE_POINT0, gConfig.RiftwalkerMoveSpeedPct, demon, true);
        if (Aura* a = demon->GetAura(SPELL_RIFTWALKER_HASTE))
        {
            a->SetDuration(int32(gConfig.RiftwalkerDurationMs));
            a->SetMaxDuration(int32(gConfig.RiftwalkerDurationMs));
        }
    }
}

class demonology_riftwalker : public PlayerScript
{
public:
    demonology_riftwalker() : PlayerScript("demonology_riftwalker") { }

    void OnPlayerSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!gConfig.Enable || !player || !spell || !spell->GetSpellInfo())
            return;
        if (spell->GetSpellInfo()->Id != SPELL_DEMONIC_CIRCLE_TELEPORT)
            return;
        if (player->getClass() != CLASS_WARLOCK || !Demonology::RiftwalkerTrained(player))
            return;

        // Warp to the circle (= the teleport destination). No circle → fall back to the player.
        float x, y, z, o;
        if (GameObject* circle = player->GetGameObject(SPELL_DEMONIC_CIRCLE_SUMMON))
        {
            x = circle->GetPositionX(); y = circle->GetPositionY();
            z = circle->GetPositionZ(); o = circle->GetOrientation();
        }
        else
        {
            x = player->GetPositionX(); y = player->GetPositionY();
            z = player->GetPositionZ(); o = player->GetOrientation();
        }

        if (Pet* pet = player->GetPet())
            RiftwalkDemon(pet, x, y, z, o);
        if (CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
        {
            for (ObjectGuid g : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*player, g))
                    RiftwalkDemon(c, x, y, z, o);
            if (Creature* gd = ObjectAccessor::GetCreature(*player, pool->GreaterDemonGuid()))
                RiftwalkDemon(gd, x, y, z, o);
        }
    }
};

void AddSC_demonology_riftwalker()
{
    new demonology_riftwalker();
}
