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
#include "FerventStandard.h"
#include "LegionEconomy.h"
#include "PetScaling.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "Util.h"

#include <list>
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
    float DemonDamageMultFull(Player* owner, bool meleeSide, bool anchorPet = false)
    {
        float mult = Demonology::DemonDamageMult(owner, meleeSide, anchorPet);

        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
            mult *= (1.0f + pool->LegionBuffDamage());

        return mult;
    }

    // Fel Blood: find an owner-cast Corruption on a unit (any rank), or nullptr.
    Aura* FindOwnerCorruption(Unit* unit, Player* owner)
    {
        for (auto const& pair : unit->GetAppliedAuras())
        {
            Aura* a = pair.second->GetBase();
            if (a && a->GetCasterGUID() == owner->GetGUID()
                && sSpellMgr->GetFirstSpellInChain(a->GetId()) == SPELL_VANILLA_CORRUPTION_R1)
                return a;
        }
        return nullptr;
    }

    // Grim Bargain: apply a damage-done buff to every commanded demon (anchor pet + legionnaires
    // + active greater demon). Amount is the buff percent (e.g. 6 for +6%).
    void ApplyToCommandedDemons(Player* owner, uint32 spellId, int32 amount)
    {
        if (Pet* pet = owner->GetPet())
            pet->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, amount, pet, true);
        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
        {
            for (ObjectGuid g : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*owner, g))
                    c->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, amount, c, true);
            if (Creature* gd = ObjectAccessor::GetCreature(*owner, pool->GreaterDemonGuid()))
                gd->CastCustomSpell(spellId, SPELLVALUE_BASE_POINT0, amount, gd, true);
        }
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

// ---- LegionEconomy: shared shard/brand entry points (declared in LegionEconomy.h) ----
namespace Demonology::LegionEconomy
{
    bool TrySoulHarvestGrant(Player* owner, float chancePct, uint32 icdMs)
    {
        if (!owner)
            return false;
        uint32 const now = getMSTime();
        auto it = g_lastProc.find(owner->GetGUID());
        if (it != g_lastProc.end() && getMSTimeDiff(it->second, now) < icdMs)
            return false;
        if (chancePct <= 0.0f || !roll_chance_f(chancePct))
            return false;
        // Check storage WITHOUT AddItem so a full bag fails silently (no client error spam) and,
        // crucially, does NOT stamp the ICD — so it re-rolls next hit instead of eating the window.
        ItemPosCountVec dest;
        if (owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_SOUL_SHARD, 1) != EQUIP_ERR_OK)
            return false;
        Item* shard = owner->StoreNewItem(dest, ITEM_SOUL_SHARD, /*update=*/true);
        if (!shard)
            return false;
        owner->SendNewItem(shard, 1, true, false);
        g_lastProc[owner->GetGUID()] = now;
        return true;
    }

    void QualifyPlayerPeriodic(Player* owner, Unit* target, float dmg, float eff)
    {
        if (!owner || eff <= 0.0f)
            return;

        // Soul Harvest: a player periodic (Fel Corruption's Corruption) rolls the SH proc at
        // baseChance x eff. NO Cruel Master acceleration (ticks don't crit through the demon path).
        if (uint8 const rank = SoulHarvestRank(owner))
            TrySoulHarvestGrant(owner, rank * gConfig.SoulHarvestChancePerRank * 100.0f * eff, gConfig.SoulHarvestInternalCdMs);

        // Doombrand: if the tick's target carries this owner's brand, charge the sigil at HALF
        // weight (DoombrandStoreMult) x eff — a deliberate secondary source, per Addendum A.2 / §6.4.
        if (target && gConfig.DoombrandStorePct > 0.0f)
            if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
                if (pool->BrandTarget() == target->GetGUID() && target->HasAura(SPELL_DOOMBRAND_DEBUFF, owner->GetGUID()))
                {
                    float const cap = gConfig.DoombrandCapSPCoef * float(PetScaling::OwnerSpellPower(owner));
                    pool->BrandAdd(dmg * gConfig.DoombrandStorePct * gConfig.FelCorruptionDoombrandStoreMult * eff, cap);
                }
    }
}

class demonology_shard_economy : public UnitScript
{
public:
    demonology_shard_economy() : UnitScript("demonology_shard_economy") { }

    void OnDamage(Unit* attacker, Unit* victim, uint32& damage) override
    {
        if (!gConfig.Enable || !attacker)
            return;

        // Grim Bargain (gb), owner half: when the WARLOCK deals damage, a chance to buff the
        // damage of every commanded demon. (The demon->owner half is in the demon branch below.)
        if (Player* p = attacker->ToPlayer())
        {
            if (p->getClass() == CLASS_WARLOCK && victim && !p->IsFriendlyTo(victim))
                if (float const ch = Demonology::GrimBargainProcChance(p))
                    if (roll_chance_f(ch * 100.0f))
                        ApplyToCommandedDemons(p, SPELL_GRIM_BARGAIN_DEMON, int32(Demonology::GrimBargainDamage(p) * 100.0f));
            return;   // the owner's own damage doesn't feed the demon-only economy below
        }

        if (!attacker->IsCreature())
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

        // Ruinous Empowerment (re) leech: while a demon is empowered, its damage heals the
        // owner for a fraction of what it deals — the army sustains you during a burst window.
        if (attacker->HasAura(SPELL_DEMONIC_EMPOWERMENT_BUFF))
            if (float const leech = Demonology::RuinousEmpowermentLeech(owner))
                owner->ModifyHealth(int32(float(damage) * leech));

        // Blood Tithe (bt): demons heal the owner for a fraction of all damage they deal
        // (doubled once you command enough of them). Always on — the standing-army sustain.
        if (owner->IsAlive())
            if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
                if (float const tithe = Demonology::BloodTitheHeal(owner, pool->CommandedDemonCount(owner)))
                    owner->ModifyHealth(int32(float(damage) * tithe));

        // Grim Bargain (gb), demon half: a demon's hit has a chance to buff YOUR damage.
        if (float const ch = Demonology::GrimBargainProcChance(owner))
            if (roll_chance_f(ch * 100.0f))
                owner->CastCustomSpell(SPELL_GRIM_BARGAIN_OWNER, SPELLVALUE_BASE_POINT0,
                    int32(Demonology::GrimBargainDamage(owner) * 100.0f), owner, true);

        // Fel Conduit (fcd): a demon's attack has a chance to grant a Conduit charge — your next
        // Shadow Bolt is instant and free (the buff's spell mods consume one charge per cast). The
        // buff builds ONE charge per proc up to 3 (the DBC clone would otherwise apply all 3 at
        // once), so it ramps like Molten Core rather than snapping to full.
        if (float const ch = Demonology::FelConduitProc(owner))
            if (roll_chance_f(ch * 100.0f))
            {
                if (Aura* a = owner->GetAura(SPELL_FEL_CONDUIT_CHARGE))
                {
                    if (a->GetCharges() < 3)
                        a->SetCharges(a->GetCharges() + 1);
                    a->RefreshDuration();
                }
                else
                {
                    owner->CastSpell(owner, SPELL_FEL_CONDUIT_CHARGE, true);
                    if (Aura* a = owner->GetAura(SPELL_FEL_CONDUIT_CHARGE))
                        a->SetCharges(1);   // fresh application starts at a single charge
                }
            }

        // Fel Blood (fb): a demon's KILLING BLOW refreshes your Corruption on nearby enemies —
        // the legion keeps your dots ticking as it mows through a pack.
        if (victim && victim->IsAlive() && damage >= victim->GetHealth()
            && Demonology::TalentRank(owner, SPELL_TALENT_FEL_BLOOD, 2))
        {
            std::list<Unit*> nearby;
            Acore::AnyUnfriendlyUnitInObjectRangeCheck check(victim, owner, 10.0f);
            Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(victim, nearby, check);
            Cell::VisitObjects(victim, searcher, 10.0f);
            for (Unit* u : nearby)
                if (u && u != victim && u->IsAlive())
                    if (Aura* cor = FindOwnerCorruption(u, owner))
                        cor->RefreshDuration();
        }

        // Doombrand (§6) accumulator: while the branded target carries our debuff, every hit
        // your demons land on it feeds a fraction (StorePct) of that damage into the sigil,
        // clamped to CapSPCoef x owner spell power. It detonates when the brand ends.
        if (victim && gConfig.DoombrandStorePct > 0.0f)
            if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
                if (pool->BrandTarget() == victim->GetGUID() && victim->HasAura(SPELL_DOOMBRAND_DEBUFF, owner->GetGUID()))
                {
                    float const cap = gConfig.DoombrandCapSPCoef * float(PetScaling::OwnerSpellPower(owner));
                    pool->BrandAdd(float(damage) * gConfig.DoombrandStorePct, cap);
                }

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

        float chancePct = rank * gConfig.SoulHarvestChancePerRank * 100.0f;
        if (cmCrit && cm >= 1)
            chancePct *= gConfig.CruelMasterProcChanceMult;

        if (Demonology::LegionEconomy::TrySoulHarvestGrant(owner, chancePct, icd) && gConfig.DebugLogShardIncome)
            LOG_INFO("module.demonology", "Soul Harvest: {} gained a Soul Shard (rank {}, {:.0f}%).",
                owner->GetName(), rank, chancePct);
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

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        float mult = 1.0f;
        if (Player* owner = Demonology::WarlockOwnerOfDemon(attacker))
        {
            mult *= DemonDamageMultFull(owner, /*meleeSide=*/true, /*anchorPet=*/attacker->ToPet() != nullptr);
            mult *= (1.0f + Demonology::BeaconOfRuinDamage(owner, attacker));   // bor: greater demons hit harder
            mult *= Demonology::FerventStandardOffenseMult(owner, attacker);    // fs: demon offense inside the standard
        }
        else if (Player* p = attacker ? attacker->ToPlayer() : nullptr)
        {
            if (p->getClass() == CLASS_WARLOCK)                                 // fs: the warlock's own hits inside the standard
                mult *= Demonology::FerventStandardOffenseMult(p, p);
        }
        // fs: a commanded demon TAKING melee damage inside the standard is mitigated (demons only).
        if (Player* dOwner = Demonology::WarlockOwnerOfDemon(target))
            mult *= Demonology::FerventStandardMitigationMult(dOwner, target);

        if (mult != 1.0f)
            damage = uint32(float(damage) * mult);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        float mult = 1.0f;
        if (Player* owner = Demonology::WarlockOwnerOfDemon(attacker))
        {
            mult *= DemonDamageMultFull(owner, /*meleeSide=*/false);
            mult *= (1.0f + Demonology::BeaconOfRuinDamage(owner, attacker));   // bor: greater demons hit harder
            // Fel Blood (fb): the succubus's Lash of Pain hits harder — the periodic legionnaire
            // signature (290515), the Command echo (290502), AND any vanilla Lash chain (7814).
            if (spellInfo && (spellInfo->Id == SPELL_EMPOWERED_LASH_OF_PAIN
                || spellInfo->Id == SPELL_SUCCUBUS_LASH
                || sSpellMgr->GetFirstSpellInChain(spellInfo->Id) == SPELL_VANILLA_LASH_OF_PAIN_R1))
                mult *= (1.0f + Demonology::FelBloodLash(owner));
            // Anchor Imp pet buff — SPEC-AGNOSTIC (no talent gate): the Imp lacks the Succubus/
            // Voidwalker CC/tank utility, so its Firebolt is brought up toward Felhunter tier for
            // EVERY spec. ToPet() is the real anchor PET (entry 416); Wild Imps / Imp legionnaires
            // are guardians (ToPet()==null) casting 290900, so their WildImp.SPCoefficient is untouched.
            if (attacker->ToPet() && attacker->GetEntry() == NPC_BASE_IMP)
                mult *= (1.0f + gConfig.ImpAnchorDamageBonusPct);
            mult *= Demonology::FerventStandardOffenseMult(owner, attacker);    // fs: demon offense inside the standard
        }
        else if (Player* p = attacker ? attacker->ToPlayer() : nullptr)
        {
            if (p->getClass() == CLASS_WARLOCK)                                 // fs: the warlock's own spells inside the standard
                mult *= Demonology::FerventStandardOffenseMult(p, p);
        }
        // fs: a commanded demon TAKING spell damage inside the standard is mitigated (demons only).
        if (Player* dOwner = Demonology::WarlockOwnerOfDemon(target))
            mult *= Demonology::FerventStandardMitigationMult(dOwner, target);

        if (mult != 1.0f)
            damage = int32(float(damage) * mult);
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
