/*
 * mod-demonology-rework — Soul Harvest shard economy (PLAN §4.1).
 *
 * A UnitScript::OnDamage hook: when a player-owned legion demon deals damage,
 * roll (per Soul Harvest rank) to grant a Soul Shard, gated by a per-player
 * internal cooldown. If the bag is full the shard is dropped WITHOUT consuming
 * the ICD, so it re-rolls next hit.
 *
 * Vertical slice: reads Soul Harvest rank from the player's known rank spells.
 * The full talent placement comes in Phase 5; the plumbing here is final.
 */
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Timer.h"

#include <unordered_map>

using namespace Demonology;

namespace
{
    // Per-player last-proc timestamp (getMSTime) — the economy's throttle.
    std::unordered_map<ObjectGuid, uint32> g_lastProc;

    uint8 SoulHarvestRank(Player* owner)
    {
        // Soul Harvest is a talent (ranks 290010-12 are consecutive marker spells). Talent
        // ranks aren't in the spell book (addToSpellBook=0), so read the talent map via
        // HasTalent; keep a HasSpell fallback for when the ranks are .learn'd directly.
        uint8 const spec = owner->GetActiveSpec();
        for (uint8 r = 3; r >= 1; --r)
        {
            uint32 const rankSpell = SPELL_SOUL_HARVEST_R1 + (r - 1);
            if (owner->HasTalent(rankSpell, spec) || owner->HasSpell(rankSpell))
                return r;
        }
        return 0;
    }
}

class demonology_shard_economy : public UnitScript
{
public:
    demonology_shard_economy() : UnitScript("demonology_shard_economy") { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!gConfig.Enable || !attacker || !attacker->IsCreature())
            return;

        Unit* ownerUnit = attacker->GetOwner();
        Player* owner = ownerUnit ? ownerUnit->ToPlayer() : nullptr;
        if (!owner)
            return;

        // Loot/reward eligibility for legion-only kills (runs regardless of Soul
        // Harvest rank, so it sits ahead of the rank gate). Core sets the loot
        // recipient to the owner for owned-creature damage, but it only credits
        // "player damage" for players and m_ControlledByPlayer/m_CreatedByPlayer
        // minions — flags our SummonCreature'd demons never get (they skip
        // SetMinion). So the mob stays loot/XP-ineligible. We fix that here:
        // OnDamage fires early in DealDamage, and LowerPlayerDamageReq sets
        // _damagedByPlayer only when it's still false, so our true wins over core's
        // later false. Guard on untagged so we never steal another player's mob.
        if (victim)
            if (Creature* creature = victim->ToCreature())
                if (creature->IsAlive())
                {
                    if (!creature->hasLootRecipient())
                        creature->SetLootRecipient(owner);
                    creature->LowerPlayerDamageReq(damage, true);
                }

        uint8 const rank = SoulHarvestRank(owner);
        if (!rank)
            return;

        uint32 const now = getMSTime();
        auto it = g_lastProc.find(owner->GetGUID());
        if (it != g_lastProc.end() && getMSTimeDiff(it->second, now) < gConfig.SoulHarvestInternalCdMs)
            return;

        float const chancePct = rank * gConfig.SoulHarvestChancePerRank * 100.0f;
        if (!roll_chance_f(chancePct))
            return;

        // Check storage WITHOUT AddItem, so a full bag fails silently — AddItem
        // would fire the "no space in your bags" client error on every proc. If it
        // doesn't fit we deliberately do NOT stamp the ICD, so it re-rolls next hit.
        ItemPosCountVec dest;
        if (owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_SOUL_SHARD, 1) != EQUIP_ERR_OK)
            return;

        if (Item* shard = owner->StoreNewItem(dest, ITEM_SOUL_SHARD, /*update=*/true))
        {
            owner->SendNewItem(shard, 1, true, false);   // normal "you received an item" feedback
            g_lastProc[owner->GetGUID()] = now;
            if (gConfig.DebugLogShardIncome)
                LOG_INFO("module.demonology", "Soul Harvest: {} gained a Soul Shard (rank {}, {:.0f}%).",
                    owner->GetName(), rank, chancePct);
        }
    }
};

// Scales the damage of every warlock-owned demon (anchor pet, legionnaires, Wild Imps)
// by the owner's demon-damage talents (Vicious Pact, Fel Armory) — uniformly for melee
// and spells, so the pet is covered the same as our own creatures. This is the single
// place the damage multiplier is applied (PetScaling/cast scripts no longer multiply).
class demonology_demon_damage : public UnitScript
{
public:
    demonology_demon_damage() : UnitScript("demonology_demon_damage") { }

    void ModifyMeleeDamage(Unit* /*target*/, Unit* attacker, uint32& damage) override
    {
        if (Player* owner = Demonology::WarlockOwnerOfDemon(attacker))
            damage = uint32(float(damage) * Demonology::DemonDamageMult(owner, /*meleeSide=*/true));
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (Player* owner = Demonology::WarlockOwnerOfDemon(attacker))
            damage = int32(float(damage) * Demonology::DemonDamageMult(owner, /*meleeSide=*/false));
    }
};

void AddSC_demonology_shard_economy()
{
    new demonology_shard_economy();
    new demonology_demon_damage();
}
