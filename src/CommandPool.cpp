/*
 * mod-demonology-rework — CommandPool implementation + mirroring driver.
 */
#include "CommandPool.h"
#include "DemonAI.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "OwnerMods.h"
#include "PetScaling.h"

#include "Creature.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "PetDefines.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraDefines.h"
#include "TemporarySummon.h"
#include "Timer.h"
#include "Util.h"

#include <algorithm>
#include <cmath>

using namespace Demonology;

namespace
{
    // Crowd control that BREAKS ON DAMAGE — attacking such a target would shatter
    // the control, so the legion must leave it alone no matter who applied it
    // (the owner's Fear, a Polymorph, a Charm...). Stun is deliberately excluded:
    // stuns don't break on damage, so a stunned mob is free to nuke.
    bool BreaksOnDamageCC(Unit* target)
    {
        return target->HasAuraType(SPELL_AURA_MOD_FEAR)
            || target->HasAuraType(SPELL_AURA_MOD_CONFUSE)      // Polymorph, etc.
            || target->HasAuraType(SPELL_AURA_MOD_CHARM)
            || target->HasAuraType(SPELL_AURA_MOD_POSSESS);
    }

    // CC applied by the ANCHOR specifically — never break the anchor's own control.
    // Covers Succubus Seduce (SPELL_AURA_MOD_STUN), which we exclude only when the
    // anchor cast it (so the player's own stuns elsewhere stay fair game).
    bool IsAnchorControlled(Unit* target, Unit* anchor)
    {
        ObjectGuid const c = anchor->GetGUID();
        return target->HasAuraTypeWithCaster(SPELL_AURA_MOD_STUN, c)          // Seduce
            || target->HasAuraTypeWithCaster(SPELL_AURA_MOD_CONFUSE, c)
            || target->HasAuraTypeWithCaster(SPELL_AURA_MOD_FEAR, c)
            || target->HasAuraTypeWithCaster(SPELL_AURA_MOD_CHARM, c)
            || target->HasAuraTypeWithCaster(SPELL_AURA_MOD_POSSESS, c);
    }

    // A target is off-limits if breaking-on-damage CC is on it (any caster) or the
    // anchor has crowd-controlled it.
    bool IsOffLimits(Unit* target, Unit* anchor)
    {
        return BreaksOnDamageCC(target) || (anchor && IsAnchorControlled(target, anchor));
    }


    // Distinct follow angle per slot (relative to owner facing) so legionnaires
    // fan out behind the owner instead of stacking on one point.
    float FormationAngle(uint32 slot)
    {
        if (slot == 0)
            return float(M_PI);                         // directly behind
        float const step = 0.6f;                        // ~34° between demons
        float const sign = (slot % 2 == 1) ? 1.0f : -1.0f;
        return float(M_PI) + sign * step * float((slot + 1) / 2);
    }

    // Sync one spellbook spell to a condition: learn it when the player should have it
    // and doesn't; remove it when they shouldn't and do. The HasSpell checks keep it a
    // no-op (no stray learn/unlearn packets) when already in the desired state.
    void SyncSpell(Player* player, uint32 spellId, bool shouldHave)
    {
        bool const has = player->HasSpell(spellId);
        if (shouldHave && !has)
            player->learnSpell(spellId);
        else if (!shouldHave && has)
            player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    // Hybrid learn (design): keep the demon summon spells in the book synced to the
    // player's talents/pets. The Summon Felguard talent teaches the Felguard PET (30146);
    // and each "Summon <type> Legionnaire" is taught once the player can summon that demon
    // AND has a command slot (Expanded Command). "Can summon" = the Felguard talent for
    // the Felguard, or knowing the normal pet-summon spell for the four basic types.
    // Uses HasTalent (our markers aren't in the spell book — see talent-effects-use-hastalent).
    void ReconcileDemonSpells(Player* player)
    {
        if (!player || player->getClass() != CLASS_WARLOCK)
            return;

        // learnSpell/removeSpell below re-fire OnPlayerLearnSpell/OnPlayerForgotSpell,
        // which call us again — guard against that re-entrancy (world update is single
        // threaded) so a single reconcile runs to completion just once.
        static bool reconciling = false;
        if (reconciling)
            return;
        reconciling = true;

        uint8 const spec = player->GetActiveSpec();
        bool const command  = player->HasTalent(SPELL_TALENT_EXPANDED_COMMAND, spec);
        bool const felguard = player->HasTalent(SPELL_TALENT_SUMMON_FELGUARD, spec);
        bool const eternal  = player->HasTalent(SPELL_TALENT_ETERNAL_SERVITUDE, spec);

        // Felguard is special: the PET itself is gated behind the Summon Felguard talent.
        SyncSpell(player, SPELL_SUMMON_FELGUARD_PET, felguard);

        struct DemonType { uint32 legionSpell; bool canSummon; };
        DemonType const types[] = {
            { SPELL_SUMMON_LEGIONNAIRE_FELGUARD,   felguard },
            { SPELL_SUMMON_LEGIONNAIRE_IMP,        player->HasSpell(SPELL_SUMMON_IMP_PET) },
            { SPELL_SUMMON_LEGIONNAIRE_VOIDWALKER, player->HasSpell(SPELL_SUMMON_VOIDWALKER_PET) },
            { SPELL_SUMMON_LEGIONNAIRE_SUCCUBUS,   player->HasSpell(SPELL_SUMMON_SUCCUBUS_PET) },
            { SPELL_SUMMON_LEGIONNAIRE_FELHOUND,   player->HasSpell(SPELL_SUMMON_FELHUNTER_PET) },
        };
        for (DemonType const& t : types)
            SyncSpell(player, t.legionSpell, t.canSummon && command);

        // Our PERMANENT greater-demon summons are unlocked by Eternal Servitude. Without
        // it, warlocks use the vanilla Inferno / Ritual of Doom (temporary) instead.
        SyncSpell(player, SPELL_SUMMON_INFERNAL,  eternal);
        SyncSpell(player, SPELL_SUMMON_DOOMGUARD, eternal);

        // ...and ES REPLACES the vanilla temp summons: remove Inferno / Ritual of Doom while
        // ES is trained and restore them on respec — but ONLY the ones the player actually
        // had (they're quest-learned, so we never grant them). State lives in the pool + DB.
        sCommandPoolMgr->GetOrCreate(player->GetGUID()).SyncVanillaGreaterDemons(player, eternal);

        reconciling = false;
    }
}

// Unified defensive threat: the first live hostile attacking the owner, the anchor pet,
// any legionnaire, or the active greater demon (skipping break-on-damage CC). Both the
// mirror and the auto-assist demon AI use this, so ANY attack on ANY of your units makes
// EVERY demon respond.
Unit* Demonology::FindLegionThreat(Player* owner)
{
    if (!owner)
        return nullptr;

    auto hostileAttacker = [](Unit* u) -> Unit*
    {
        if (!u)
            return nullptr;
        for (Unit* a : u->getAttackers())
            if (a && a->IsAlive() && !u->IsFriendlyTo(a) && !BreaksOnDamageCC(a))
                return a;
        return nullptr;
    };

    if (Unit* a = hostileAttacker(owner))            return a;
    if (Unit* a = hostileAttacker(owner->GetPet()))  return a;

    if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
    {
        for (ObjectGuid guid : pool->Legionnaires())
            if (Unit* a = hostileAttacker(ObjectAccessor::GetCreature(*owner, guid)))
                return a;
        if (Unit* a = hostileAttacker(ObjectAccessor::GetCreature(*owner, pool->GreaterDemonGuid())))
            return a;
    }
    return nullptr;
}

// --------------------------------------------------------------- CommandPool

uint8 CommandPool::GetMaxLegionnaires() const
{
    // Base slots + the Command talent spine: Expanded Command (2nd), Expanded
    // Command II (3rd), Legion Commander (4th). Each is a learned passive, so
    // HasSpell(rank1) == the talent is trained. Clamped to the hard cap.
    uint8 cap = gConfig.PoolBaseLegionnaires;
    if (Player* owner = ObjectAccessor::FindPlayer(_owner))
    {
        // Talent ranks are NOT added to the spell book (our talents have Talent.dbc
        // addToSpellBook=0, and Player::LearnTalent only calls learnSpell when that
        // flag is set and the rank isn't passive). So HasSpell(rank) is always false;
        // HasTalent reads the talent map that a spent point always fills.
        uint8 const spec = owner->GetActiveSpec();
        if (owner->HasTalent(SPELL_TALENT_EXPANDED_COMMAND, spec))    ++cap;
        if (owner->HasTalent(SPELL_TALENT_EXPANDED_COMMAND_II, spec)) ++cap;
        if (owner->HasTalent(SPELL_TALENT_LEGION_COMMANDER, spec))    ++cap;
    }
    return std::min(cap, gConfig.PoolMaxLegionnaires);
}

uint8 CommandPool::LegionnaireCap() const
{
    // An active greater demon (Infernal/Doomguard) occupies command slots, leaving
    // fewer for legionnaires — so a heavy demon can't stack on a full legion.
    uint8 const total = GetMaxLegionnaires();
    return total > _greaterDemonSlots ? uint8(total - _greaterDemonSlots) : uint8(0);
}

void CommandPool::RegisterGreaterDemon(Player* owner, Creature* demon, uint8 slotCost)
{
    // One greater demon at a time — despawn any existing one first (frees its slots).
    if (Creature* old = ObjectAccessor::GetCreature(*owner, _greaterDemonGuid))
        old->DespawnOrUnsummon();
    _greaterDemonGuid.Clear();
    _greaterDemonSlots = 0;

    // Reserve the slots: evict the oldest legionnaires until they fit alongside it.
    uint8 const legCap = GetMaxLegionnaires() > slotCost ? uint8(GetMaxLegionnaires() - slotCost) : uint8(0);
    while (_legionnaires.size() > legCap)
    {
        ObjectGuid oldest = _legionnaires.front();
        _legionnaires.erase(_legionnaires.begin());
        if (Creature* c = ObjectAccessor::GetCreature(*owner, oldest))
            c->DespawnOrUnsummon();
    }

    _greaterDemonGuid = demon->GetGUID();
    _greaterDemonSlots = slotCost;
    OnPoolChanged();
}

void CommandPool::SyncVanillaGreaterDemons(Player* owner, bool eternal)
{
    if (!owner)
        return;

    ObjectGuid::LowType const g = owner->GetGUID().GetCounter();

    // Lazy-load (once per session) the vanilla summons WE removed for this character.
    if (!_esRemovedLoaded)
    {
        _esRemovedLoaded = true;
        if (QueryResult res = CharacterDatabase.Query("SELECT spell FROM character_demonology_es_removed WHERE guid = {}", g))
            do { _esRemoved.push_back(res->Fetch()[0].Get<uint32>()); } while (res->NextRow());
    }

    static constexpr uint32 VANILLA[] = { SPELL_VANILLA_INFERNO, SPELL_VANILLA_RITUAL_OF_DOOM };

    if (eternal)
    {
        // Remove any the player currently KNOWS (never grant), remembering it so we can
        // give it back — these are quest-learned and can't be re-trained.
        for (uint32 sp : VANILLA)
            if (owner->HasSpell(sp))
            {
                owner->removeSpell(sp, SPEC_MASK_ALL, false);
                if (std::find(_esRemoved.begin(), _esRemoved.end(), sp) == _esRemoved.end())
                {
                    _esRemoved.push_back(sp);
                    CharacterDatabase.Execute("REPLACE INTO character_demonology_es_removed (guid, spell) VALUES ({}, {})", g, sp);
                }
            }
    }
    else if (!_esRemoved.empty())
    {
        // Restore ONLY what we previously removed (so a warlock who never had them gets nothing).
        for (uint32 sp : _esRemoved)
            owner->learnSpell(sp);
        _esRemoved.clear();
        CharacterDatabase.Execute("DELETE FROM character_demonology_es_removed WHERE guid = {}", g);
    }
}

void CommandPool::Add(Creature* legionnaire)
{
    if (!legionnaire)
        return;

    // Evict the oldest occupant(s) until there's room (design §2).
    while (!_legionnaires.empty() && _legionnaires.size() >= LegionnaireCap())
    {
        ObjectGuid oldest = _legionnaires.front();
        _legionnaires.erase(_legionnaires.begin());
        if (Creature* c = ObjectAccessor::GetCreature(*legionnaire, oldest))
            c->DespawnOrUnsummon();
    }

    _legionnaires.push_back(legionnaire->GetGUID());
    OnPoolChanged();
}

void CommandPool::Remove(ObjectGuid guid)
{
    for (auto it = _legionnaires.begin(); it != _legionnaires.end(); ++it)
        if (*it == guid)
        {
            _legionnaires.erase(it);
            OnPoolChanged();
            return;
        }
}

void CommandPool::DismissAll()
{
    Player* owner = ObjectAccessor::FindPlayer(_owner);

    // The active greater demon (Infernal/Doomguard) — it's tracked separately from the
    // legionnaires, so dismiss it here too (fixes `.legion dismiss` leaving it behind).
    if (owner)
        if (Creature* gd = ObjectAccessor::GetCreature(*owner, _greaterDemonGuid))
            gd->DespawnOrUnsummon();
    _greaterDemonGuid.Clear();
    _greaterDemonSlots = 0;

    if (owner)
        for (ObjectGuid guid : _legionnaires)
            if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
                c->DespawnOrUnsummon();

    _legionnaires.clear();
    OnPoolChanged();
}

Creature* CommandPool::Recruit(Player* owner, uint32 entry, float healthPct)
{
    // No free command slots (no Expanded Command, or a greater demon fills them all) →
    // nothing to command. Refuse outright — covers summon spells, login restore, and
    // `.legion recruit`. (base = 0; design: talents grant slots.)
    if (LegionnaireCap() == 0)
        return nullptr;

    // Spread the spawn around the owner so recruits don't stack; the mirror keeps
    // them fanned out thereafter.
    float const angle = owner->GetOrientation() + float(M_PI) + (float(_legionnaires.size()) - 1.0f) * 0.6f;
    float const dist = 2.5f;
    float const x = owner->GetPositionX() + std::cos(angle) * dist;
    float const y = owner->GetPositionY() + std::sin(angle) * dist;

    Creature* c = owner->SummonCreature(entry, x, y, owner->GetPositionZ(), owner->GetOrientation(),
                                        TEMPSUMMON_MANUAL_DESPAWN, 0);
    if (!c)
        return nullptr;

    c->SetOwnerGUID(owner->GetGUID());
    c->SetCreatorGUID(owner->GetGUID());                   // client attributes its damage to the owner
    c->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);           // shows in the owner's combat log / floating text
    c->SetFaction(owner->GetFaction());
    c->SetLevel(owner->GetLevel());                        // sets raw creature-tier stats
    c->SetReactState(REACT_DEFENSIVE);

    // No-evade guardian AI. The Imp is a ranged caster (its whole identity is
    // Firebolt) — give it the caster mode with the SP-scaling Wild Imp Firebolt;
    // every other demon type melees. (Richer per-type signatures come later.)
    if (entry == NPC_BASE_IMP)
        c->AIM_Initialize(new GuardianAttackerAI(c, /*autoAssist=*/false, SPELL_WILD_IMP_FIREBOLT, /*castCooldownMs=*/2000));
    else
        c->AIM_Initialize(new GuardianAttackerAI(c));

    // Stat inheritance (Phase 6): scale health + melee off the owner's spell power,
    // replacing the old flat power clamp so legionnaires track the owner's gear.
    PetScaling::ApplyInheritance(owner, c);
    _lastAppliedSP = PetScaling::OwnerSpellPower(owner);   // seed so the mirror doesn't needlessly re-sync

    if (healthPct > 0.0f && healthPct < 1.0f)
        c->SetHealth(uint32(float(c->GetMaxHealth()) * healthPct));

    Add(c);                                                // evicts the oldest if full
    return c;
}

void CommandPool::Save() const
{
    // A login restore is still pending — the DB rows are already correct, don't clobber.
    if (_legionnaires.empty() && !_restore.empty())
        return;

    ObjectGuid::LowType const g = _owner.GetCounter();
    CharacterDatabase.Execute("DELETE FROM character_legion_slots WHERE guid = {}", g);

    Player* owner = ObjectAccessor::FindPlayer(_owner);
    if (!owner)
        return;

    uint8 slot = 1;
    for (ObjectGuid guid : _legionnaires)
    {
        Creature* c = ObjectAccessor::GetCreature(*owner, guid);
        if (!c || !c->IsAlive())
            continue;
        float const hp = c->GetMaxHealth() ? float(c->GetHealth()) / float(c->GetMaxHealth()) : 1.0f;
        CharacterDatabase.Execute(
            "INSERT INTO character_legion_slots (guid, slot, creature_entry, saved_health_pct) VALUES ({}, {}, {}, {})",
            g, uint32(slot), c->GetEntry(), hp);
        ++slot;
    }
}

void CommandPool::QueueRestore(uint32 entry, float healthPct)
{
    _restore.push_back({ entry, healthPct });
    _restoreTimer = 0;
}

void CommandPool::DoRestore(Player* owner)
{
    for (PendingDemon const& d : _restore)
        Recruit(owner, d.entry, d.healthPct);
    _restore.clear();
    _restoreTimer = 0;
}

void CommandPool::StashForTeleport(Player* owner, uint32 targetMapId)
{
    // Only a cross-map teleport orphans our summons (the core anchor pet follows either
    // way). Intra-map teleports leave the legion where it is — the mirror re-forms it.
    if (!owner || targetMapId == owner->GetMapId())
        return;

    // Legionnaires: capture entry + health%, then despawn (reachable — still on the old
    // map here), so they resummon on the new map instead of lingering as idle orphans.
    for (ObjectGuid guid : _legionnaires)
        if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
        {
            float const hp = c->GetMaxHealth() ? float(c->GetHealth()) / float(c->GetMaxHealth()) : 1.0f;
            _restore.push_back({ c->GetEntry(), hp });
            c->DespawnOrUnsummon();
        }
    _legionnaires.clear();

    // Greater demon: stash its entry and despawn it too (was previously left behind as a
    // "zombie" that reappeared on return). It resummons first on the new map (slots first).
    if (!_greaterDemonGuid.IsEmpty())
    {
        if (Creature* gd = ObjectAccessor::GetCreature(*owner, _greaterDemonGuid))
        {
            _restoreGreaterDemonEntry = gd->GetEntry();
            gd->DespawnOrUnsummon();
        }
        _greaterDemonGuid.Clear();
        _greaterDemonSlots = 0;
    }

    _restoreTimer = 0;
    OnPoolChanged();
}

void CommandPool::Update(uint32 diff)
{
    Player* owner = ObjectAccessor::FindPlayer(_owner);
    if (!owner || !owner->IsInWorld())
        return;

    // Deferred Felguard-spell re-sync after a respec: OnPlayerTalentsReset fires BEFORE
    // the talents are actually cleared (and the reset can still abort), so we defer to
    // here where HasTalent reflects the settled state.
    if (_reconcileTalents)
    {
        _reconcileTalents = false;
        ReconcileDemonSpells(owner);
        PetScaling::ReapplyAll(owner);          // a respec changed fc/vp/si/fa — re-scale demons
    }

    // Free the greater demon's reserved command slots once it's gone (timed despawn,
    // death, or dismiss). Our permanent greater demon is ES-gated, so also dismiss it if
    // the owner respecs OUT of Eternal Servitude — it shouldn't linger without the talent.
    if (_greaterDemonSlots > 0)
    {
        Creature* gd = ObjectAccessor::GetCreature(*owner, _greaterDemonGuid);
        bool const lostEternal = !owner->HasTalent(SPELL_TALENT_ETERNAL_SERVITUDE, owner->GetActiveSpec());
        if (!gd || !gd->IsAlive() || lostEternal)
        {
            if (gd && lostEternal)
                gd->DespawnOrUnsummon();
            _greaterDemonGuid.Clear();
            _greaterDemonSlots = 0;
            OnPoolChanged();
        }
    }

    // Stash + despawn the legion while the owner is DEAD (don't trail the corpse/
    // ghost) or MOUNTED (like pets — so they don't aggro while travelling). It
    // resummons ~2s after the owner is alive-and-unmounted, via the restore path below.
    if (!owner->IsAlive() || owner->IsMounted())
    {
        if (!_legionnaires.empty())
        {
            for (ObjectGuid guid : _legionnaires)
                if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
                {
                    float const hp = c->GetMaxHealth() ? float(c->GetHealth()) / float(c->GetMaxHealth()) : 1.0f;
                    _restore.push_back({ c->GetEntry(), hp });
                    c->DespawnOrUnsummon();
                }
            _legionnaires.clear();
            _restoreTimer = 0;
            OnPoolChanged();
        }
        return;
    }

    // Restore queued demons a couple of seconds after login / a cross-map teleport
    // (summoning during the transition is fragile — PLAN §3.4). The greater demon goes
    // first so it reserves its command slots before the legionnaires fill the rest.
    if (!_restore.empty() || _restoreGreaterDemonEntry)
    {
        _restoreTimer += diff;
        if (_restoreTimer >= 2000)
        {
            if (_restoreGreaterDemonEntry)
            {
                Demonology::SummonGreaterDemon(owner, _restoreGreaterDemonEntry);
                _restoreGreaterDemonEntry = 0;
            }
            DoRestore(owner);
        }
        return;
    }

    // Demonic Rebirth (dr): resummon legionnaires queued by a death this tick. The owner
    // is alive/unmounted/not-restoring here. Prune any dead entries first so the freed
    // slot is actually available, then refill up to the cap (never evicting a survivor).
    if (!_pendingRebirth.empty())
    {
        _legionnaires.erase(std::remove_if(_legionnaires.begin(), _legionnaires.end(),
            [&](ObjectGuid g) { Creature* c = ObjectAccessor::GetCreature(*owner, g); return !c || !c->IsAlive(); }),
            _legionnaires.end());
        for (uint32 entry : _pendingRebirth)
            if (_legionnaires.size() < LegionnaireCap())
                Recruit(owner, entry);
        _pendingRebirth.clear();
        OnPoolChanged();
    }

    // A respec that drops a Command talent lowers the cap; evict the oldest
    // legionnaire(s) until we're within it (you can't command past the cap).
    while (_legionnaires.size() > LegionnaireCap())
    {
        ObjectGuid oldest = _legionnaires.front();
        _legionnaires.erase(_legionnaires.begin());
        if (Creature* c = ObjectAccessor::GetCreature(*owner, oldest))
            c->DespawnOrUnsummon();
        OnPoolChanged();
    }

    _mirrorTimer += diff;
    if (_mirrorTimer < 300)                 // batched cadence (PLAN §10.3)
        return;
    _mirrorTimer = 0;

    Mirror(owner);
}

void CommandPool::Mirror(Player* owner)
{
    // Keep the anchor pet's talent buffs (health/attack-speed) in sync — idempotent,
    // so this also picks up a freshly (re)summoned pet within a tick. Runs even for a
    // warlock with only a pet (pool created on login); the rest is legionnaire-only.
    PetScaling::ApplyPetMods(owner);

    // Owner auras from the current composition (op per-demon HP/haste, cv owner stamina,
    // Legion Aura toggle). Runs even for a pet-only warlock, ahead of the empty-return.
    OwnerMods::Apply(owner, CommandedDemonCount(owner));

    if (_legionnaires.empty())
        return;

    // Bound by Blood haste is a transient window; refresh legionnaire attack speed each
    // tick while it's active, plus one tick after it lapses so the haste cleanly reverts.
    bool const buffActive = LegionBuffHaste() > 0.0f;
    bool const refreshHaste = buffActive || _lastLegionBuffActive;
    _lastLegionBuffActive = buffActive;

    // The anchor is the real Pet; legionnaires mirror its stance and target
    // (PLAN §3.1). Pet derives from Creature, so it has GetReactState().
    Pet* anchor = owner->GetPet();

    // (1) React-state mirroring: legionnaires inherit the anchor's stance.
    ReactStates const react = anchor ? anchor->GetReactState() : REACT_DEFENSIVE;

    // Legionnaires follow the ANCHOR (not the owner): pet "Stay" → they hold with it,
    // pet "Follow" → they trail near the owner, and after combat they return to it.
    // Falls back to the owner when there's no anchor. Re-issue the follow only when
    // the follow target actually changes, so we don't fight the generator each tick.
    Unit* const followTarget = anchor ? static_cast<Unit*>(anchor) : static_cast<Unit*>(owner);
    bool const followChanged = (followTarget->GetGUID() != _followTargetGuid);
    _followTargetGuid = followTarget->GetGUID();

    // Re-fan the WHOLE formation when the roster grows/shrinks (a new summon or a
    // death) or the follow target changes. Without this, a freshly-summoned legionnaire
    // that spawned already following the owner keeps the core default follow angle
    // (PET_FOLLOW_ANGLE, identical for all) and the pack stacks on one point — the
    // once-only "not already following" check never overrides that default.
    bool const reapplyFormation = followChanged || (_legionnaires.size() != _lastFormationCount);

    // The target to mirror — the anchor's victim (or the owner's), unless the
    // anchor is passive, or the anchor has crowd-controlled that target.
    Unit* target = nullptr;
    if (react != REACT_PASSIVE)
    {
        target = anchor ? anchor->GetVictim() : nullptr;
        if (!target)
            target = owner->GetVictim();

        // (2) CC-awareness: never attack a crowd-controlled target — the anchor's
        // Seduce OR the owner's own Fear/Polymorph (which would break on the hit).
        // Fall back to the owner's target if it's clear, else stand down.
        if (target && IsOffLimits(target, anchor))
        {
            Unit* alt = owner->GetVictim();
            target = (alt && alt != target && !IsOffLimits(alt, anchor)) ? alt : nullptr;
        }

        // Defensive/aggressive: with nothing commanded, retaliate against whatever is
        // attacking the owner OR any of their demons (unified with the auto-assist AI;
        // never a CC'd target).
        if (!target)
            if (Unit* threat = FindLegionThreat(owner))
                if (!IsOffLimits(threat, anchor))
                    target = threat;
    }

    // Keep inheritance in sync with the owner's live spell power (gear, buffs,
    // trinket procs, talents) — the cached melee/health fields are only re-derived
    // when SP moves, so this stays cheap. (Firebolt damage is already live.)
    int32 const ownerSP = PetScaling::OwnerSpellPower(owner);
    bool const resync = (ownerSP != _lastAppliedSP);
    _lastAppliedSP = ownerSP;

    std::vector<ObjectGuid> alive;
    alive.reserve(_legionnaires.size());
    uint32 slot = 0;

    for (ObjectGuid guid : _legionnaires)
    {
        Creature* c = ObjectAccessor::GetCreature(*owner, guid);
        if (!c || !c->IsAlive())
            continue;                       // prune despawned/dead
        alive.push_back(guid);

        if (resync)
            PetScaling::ApplyInheritance(owner, c);
        else if (refreshHaste)
            PetScaling::ReapplyAttackSpeed(owner, c);   // transient bbb haste on/off

        c->SetReactState(react);            // (1) mirror the anchor's stance

        // Attack hostile OR neutral (skip friendly). Legionnaires carry the no-evade
        // GuardianAttackerAI (set at recruit), so AttackStart persists and lands the
        // first hit even on a neutral target instead of dropping it.
        if (target && !c->IsFriendlyTo(target))
        {
            if (c->GetVictim() != target)
                c->AI()->AttackStart(target);
        }
        else
        {
            if (c->GetVictim())
            {
                c->AttackStop();
                c->CombatStop(true);
            }
            // (3) formation: fan out so the pack never stacks on one point. Stagger
            // the follow distance per slot as well, so they separate clearly even at
            // the tight pet follow range.
            if (reapplyFormation || c->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                c->GetMotionMaster()->MoveFollow(followTarget, PET_FOLLOW_DIST + float(slot) * 0.75f, FormationAngle(slot));
        }
        ++slot;
    }

    if (alive.size() != _legionnaires.size())
    {
        _legionnaires.swap(alive);
        OnPoolChanged();
    }
    _lastFormationCount = _legionnaires.size();
}

void CommandPool::OnPoolChanged()
{
    // Single source of truth for everything pool-size-dependent (DESIGN_V2 §8.1).
    // Recompute the owner auras from the new composition: Overlord's Presence
    // (per-demon owner HP/haste), Cursed Vitality's owner-stamina half, and the
    // Grand Warlock's Design Legion Aura toggle (groundwork; full rider in Phase 5).
    if (Player* owner = ObjectAccessor::FindPlayer(_owner))
        OwnerMods::Apply(owner, CommandedDemonCount(owner));
}

uint8 CommandPool::CommandedDemonCount(Player* owner) const
{
    if (!owner)
        return 0;
    uint8 count = uint8(_legionnaires.size());
    if (owner->GetPet())
        ++count;                            // the anchor
    if (!_greaterDemonGuid.IsEmpty())
        ++count;                            // active Infernal/Doomguard
    return count;
}

void CommandPool::TriggerLegionBuff(float dmgFrac, float hasteFrac, uint32 durMs)
{
    _legionBuffDmg = dmgFrac;
    _legionBuffHaste = hasteFrac;
    _legionBuffUntilMs = getMSTime() + durMs;
}

float CommandPool::LegionBuffDamage() const
{
    return (_legionBuffUntilMs && getMSTime() < _legionBuffUntilMs) ? _legionBuffDmg : 0.0f;
}

float CommandPool::LegionBuffHaste() const
{
    return (_legionBuffUntilMs && getMSTime() < _legionBuffUntilMs) ? _legionBuffHaste : 0.0f;
}

void CommandPool::OnDemonDeath(Player* owner, Creature* dead)
{
    if (!owner || !dead)
        return;

    bool const wasLegionnaire =
        std::find(_legionnaires.begin(), _legionnaires.end(), dead->GetGUID()) != _legionnaires.end();

    // --- Bound by Blood (bbb): the survivors gain a transient damage/haste buff and the
    // owner refunds a Soul Shard ("demon deaths fund your actives" under Path B). The buff
    // is a strong bloodlust, so it's on its own cooldown; the shard refund is NOT gated. ---
    if (float const dmg = Demonology::BoundByBloodDamage(owner))
    {
        uint32 const now = getMSTime();
        if (now >= _bloodBuffReadyAtMs)
        {
            TriggerLegionBuff(dmg, Demonology::BoundByBloodHaste(owner), gConfig.BoundByBloodDurationMs);
            _bloodBuffReadyAtMs = now + gConfig.BoundByBloodIcdMs;
        }

        if (gConfig.BoundByBloodRefundShard)
        {
            ItemPosCountVec dest;
            if (owner->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_SOUL_SHARD, 1) == EQUIP_ERR_OK)
                if (Item* shard = owner->StoreNewItem(dest, ITEM_SOUL_SHARD, /*update=*/true))
                    owner->SendNewItem(shard, 1, true, false);
        }
    }

    // --- Demonic Rebirth (dr): chance to instantly resummon a dying LEGIONNAIRE (the
    // anchor pet / greater demon are re-summoned by their own spells, not here), on a
    // shared ICD. Queue the entry; Update summons it next tick (summoning inside the
    // death callback is unsafe). ---
    if (wasLegionnaire)
        if (float const chance = Demonology::DemonicRebirthChance(owner))
        {
            uint32 const now = getMSTime();
            if (now >= _rebirthReadyAtMs && roll_chance_f(chance * 100.0f))
            {
                _pendingRebirth.push_back(dead->GetEntry());
                _rebirthReadyAtMs = now + gConfig.DemonicRebirthIcdMs;
            }
        }

    // The dead legionnaire is pruned from _legionnaires by the next Mirror tick (which
    // also fires OnPoolChanged); nothing to erase here.
}

uint32 CommandPool::RebirthReadyInMs() const
{
    uint32 const now = getMSTime();
    return now >= _rebirthReadyAtMs ? 0u : _rebirthReadyAtMs - now;
}

uint32 CommandPool::BloodBuffReadyInMs() const
{
    uint32 const now = getMSTime();
    return now >= _bloodBuffReadyAtMs ? 0u : _bloodBuffReadyAtMs - now;
}

void Demonology::NotifyDemonDeath(Player* owner, Creature* dead)
{
    if (!owner)
        return;
    if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
        pool->OnDemonDeath(owner, dead);
}

// ------------------------------------------------------------ CommandPoolMgr

CommandPoolMgr* CommandPoolMgr::instance()
{
    static CommandPoolMgr mgr;
    return &mgr;
}

CommandPool* CommandPoolMgr::Find(ObjectGuid player)
{
    auto it = _pools.find(player);
    return it == _pools.end() ? nullptr : &it->second;
}

CommandPool& CommandPoolMgr::GetOrCreate(ObjectGuid player)
{
    return _pools.try_emplace(player, player).first->second;
}

void CommandPoolMgr::Remove(ObjectGuid player)
{
    auto it = _pools.find(player);
    if (it == _pools.end())
        return;
    it->second.DismissAll();
    _pools.erase(it);
}

void CommandPoolMgr::Update(uint32 diff)
{
    for (auto& [guid, pool] : _pools)
        pool.Update(diff);
}

// ------------------------------------------------------------------- scripts

class demonology_pool_worldscript : public WorldScript
{
public:
    demonology_pool_worldscript() : WorldScript("demonology_pool_worldscript") { }

    void OnUpdate(uint32 diff) override
    {
        if (gConfig.Enable)
            sCommandPoolMgr->Update(diff);
    }
};

class demonology_pool_playerscript : public PlayerScript
{
public:
    demonology_pool_playerscript() : PlayerScript("demonology_pool_playerscript") { }

    void OnPlayerLogin(Player* player) override
    {
        // Hybrid learn: keep the Felguard pet/legionnaire spells in the book matching
        // the player's talents. Login state is accurate, so reconcile immediately.
        ReconcileDemonSpells(player);

        // Ensure a pool exists for any warlock so CommandPool::Update ticks and keeps the
        // anchor pet's talent buffs synced, even with no legionnaires out.
        if (player->getClass() == CLASS_WARLOCK)
            sCommandPoolMgr->GetOrCreate(player->GetGUID());

        // Queue any saved legionnaires; CommandPool::Update summons them ~2s later.
        QueryResult res = CharacterDatabase.Query(
            "SELECT creature_entry, saved_health_pct FROM character_legion_slots WHERE guid = {} ORDER BY slot",
            player->GetGUID().GetCounter());
        if (!res)
            return;

        CommandPool& pool = sCommandPoolMgr->GetOrCreate(player->GetGUID());
        do
        {
            Field* f = res->Fetch();
            pool.QueueRestore(f[0].Get<uint32>(), f[1].Get<float>());
        } while (res->NextRow());
    }

    // A talent was learned — talent state is already updated here. Re-sync the summon
    // spells AND re-apply demon stat inheritance (fc/vp/si/fa read talents, so existing
    // demons must be re-scaled now, not only on the next summon/gear change).
    void OnPlayerLearnTalents(Player* player, uint32 /*talentId*/, uint32 /*talentRank*/, uint32 /*spellid*/) override
    {
        ReconcileDemonSpells(player);
        Demonology::PetScaling::ReapplyAll(player);
    }

    // A respec wipes the active spec's talents; the hook fires BEFORE the wipe (and the
    // reset can abort), so defer the re-sync to the next pool tick.
    void OnPlayerTalentsReset(Player* player, bool /*noCost*/) override
    {
        sCommandPoolMgr->GetOrCreate(player->GetGUID()).QueueReconcile();
    }

    // Fires BEFORE a teleport, while the legion is still on the old map and reachable. A
    // cross-map teleport orphans our summons (only the core anchor pet follows), so stash +
    // despawn the whole legion here; CommandPool::Update resummons it on the new map.
    bool OnPlayerBeforeTeleport(Player* player, uint32 mapid, float /*x*/, float /*y*/, float /*z*/,
        float /*orientation*/, uint32 /*options*/, Unit* /*target*/) override
    {
        if (CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
            pool->StashForTeleport(player, mapid);
        return true;                                    // never block the teleport
    }

    // Learning/unlearning a base pet-summon spell (trainer/quest) changes which legionnaire
    // types are available, so re-sync. ReconcileDemonSpells guards its own re-entrancy
    // (these hooks re-fire when it learns/removes a legionnaire spell).
    void OnPlayerLearnSpell(Player* player, uint32 /*spellID*/) override
    {
        ReconcileDemonSpells(player);
    }

    void OnPlayerForgotSpell(Player* player, uint32 /*spellID*/) override
    {
        ReconcileDemonSpells(player);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
            pool->Save();                               // persist before teardown
        sCommandPoolMgr->Remove(player->GetGUID());     // clean up legionnaires on logout/disconnect
        Demonology::PetScaling::ForgetPet(player->GetGUID());
        Demonology::OwnerMods::Clear(player->GetGUID()); // owner unit-mods die with the unit; drop bookkeeping
    }

    // Gear changes move the owner's spell power, so re-run inheritance on every
    // owned demon (melee/health are cached fields; Firebolt damage is already live).
    void OnPlayerEquip(Player* player, Item* /*it*/, uint8 /*bag*/, uint8 /*slot*/, bool /*update*/) override
    {
        Demonology::PetScaling::ReapplyAll(player);
    }

    void OnPlayerUnequip(Player* player, Item* /*it*/) override
    {
        Demonology::PetScaling::ReapplyAll(player);
    }
};

void AddSC_demonology_command_pool()
{
    new demonology_pool_worldscript();
    new demonology_pool_playerscript();
}
