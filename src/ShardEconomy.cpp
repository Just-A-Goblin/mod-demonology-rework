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
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "Item.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "Util.h"

#include <unordered_map>

using namespace Demonology;

namespace
{
    // Per-player last-proc timestamp (getMSTime) — the economy's throttle.
    std::unordered_map<ObjectGuid, uint32> g_lastProc;

    // The demon-damage multiplier applied to any warlock-owned demon's hit: the scaling
    // talents (Vicious Pact / Fel Armory) and the transient Bound by Blood survivor buff.
    // Pactbound Fury is NOT here anymore — it's a REAL crit on both sides now (melee via
    // OnBeforeRollMeleeOutcomeAgainst, spell via the demon's base spell crit chance set in
    // PetScaling), so it shows as genuine yellow crits instead of an invisible multiply.
    float DemonDamageMultFull(Player* owner, bool meleeSide)
    {
        float mult = Demonology::DemonDamageMult(owner, meleeSide);

        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
            mult *= (1.0f + pool->LegionBuffDamage());

        return mult;
    }

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

        // Voidwalkers off-tank (§ change): they generate extra threat from all their damage
        // so what they hit sticks to them (their Command self-shield only matters if they're
        // being attacked). Add the multiplier's surplus on top of the core's own threat.
        if (victim && attacker->GetEntry() == NPC_BASE_VOIDWALKER && gConfig.VoidwalkerThreatMultiplier > 1.0f)
            victim->GetThreatMgr().AddThreat(attacker, float(damage) * (gConfig.VoidwalkerThreatMultiplier - 1.0f));

        uint8 const rank = SoulHarvestRank(owner);
        if (!rank)
            return;

        // Cruel Master: simulate whether this demon hit crit (no server hook reports the
        // real crit result), at the demon's crit chance. On a sim-crit, rank 1 multiplies
        // the proc chance and rank 2 also shortens the effective ICD.
        uint8 const cm = Demonology::CruelMasterRank(owner);
        bool const cmCrit = cm && roll_chance_f(Demonology::DemonSimCritChance(owner) * 100.0f);

        uint32 icd = gConfig.SoulHarvestInternalCdMs;
        if (cmCrit && cm >= 2)
            icd = uint32(float(icd) * gConfig.CruelMasterIcdMultOnCrit);

        uint32 const now = getMSTime();
        auto it = g_lastProc.find(owner->GetGUID());
        if (it != g_lastProc.end() && getMSTimeDiff(it->second, now) < icd)
            return;

        float chancePct = rank * gConfig.SoulHarvestChancePerRank * 100.0f;
        if (cmCrit && cm >= 1)
            chancePct *= gConfig.CruelMasterProcChanceMult;
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
            damage = uint32(float(damage) * DemonDamageMultFull(owner, /*meleeSide=*/true));
    }

    void ModifySpellDamageTaken(Unit* /*target*/, Unit* attacker, int32& damage, SpellInfo const* /*spellInfo*/) override
    {
        if (Player* owner = Demonology::WarlockOwnerOfDemon(attacker))
            damage = int32(float(damage) * DemonDamageMultFull(owner, /*meleeSide=*/false));
    }

    // Pactbound Fury (melee): inject the talent crit chance into the core's melee-outcome
    // roll so a warlock-owned demon throws a REAL crit (visible yellow hit, core's 2x). The
    // crit_chance here is in 0.01% units (rolled against 0..10000), so fraction x 10000.
    void OnBeforeRollMeleeOutcomeAgainst(Unit const* attacker, Unit const* /*victim*/, WeaponAttackType /*attType*/,
        int32& /*attackerMaxSkillValueForLevel*/, int32& /*victimMaxSkillValueForLevel*/,
        int32& /*attackerWeaponSkill*/, int32& /*victimDefenseSkill*/, int32& crit_chance,
        int32& /*miss_chance*/, int32& /*dodge_chance*/, int32& /*parry_chance*/, int32& /*block_chance*/) override
    {
        if (Player* owner = Demonology::WarlockOwnerOfDemon(const_cast<Unit*>(attacker)))
            crit_chance += int32(Demonology::PactboundFuryCritChance(owner) * 10000.0f);
    }
};

void AddSC_demonology_shard_economy()
{
    new demonology_shard_economy();
    new demonology_demon_damage();
}
