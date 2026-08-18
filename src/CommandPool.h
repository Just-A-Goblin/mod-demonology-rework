/*
 * mod-demonology-rework — CommandPool (Phase 1).
 *
 * One pool per player. Slot 0 is the anchor (the real Pet); this tracks the
 * mirroring legionnaire guardians (slots 1..N) and drives their mirroring on a
 * batched cadence (PLAN §3.1, §3.4, §10.3): legionnaires attack what the anchor
 * attacks and follow the owner when idle. Commanded as one unit.
 *
 * Increment 1 is in-memory (rebuilt empty on login). Persistence via
 * character_legion_slots is the next increment.
 */
#ifndef MOD_DEMONOLOGY_REWORK_COMMAND_POOL_H
#define MOD_DEMONOLOGY_REWORK_COMMAND_POOL_H

#include "Define.h"
#include "ObjectGuid.h"

#include <unordered_map>
#include <vector>

class Player;
class Creature;
class Unit;

namespace Demonology
{
    // Any live hostile currently attacking the owner or one of their demons (anchor pet,
    // legionnaires, or the active greater demon), skipping break-on-damage CC. Drives the
    // UNIFIED defensive response: any attack on any of your units rallies every demon —
    // used by both the legionnaire mirror and the auto-assist demon AI (DemonAI.h).
    Unit* FindLegionThreat(Player* owner);

    // Fired from GuardianAttackerAI::JustDied — a REAL death of an owned demon (dismiss/
    // stash/logout use DespawnOrUnsummon, which never calls JustDied). Routes to the
    // owner's pool for the demon-death consumers (Demonic Rebirth + Bound by Blood).
    void NotifyDemonDeath(Player* owner, Creature* dead);

    class CommandPool
    {
    public:
        explicit CommandPool(ObjectGuid owner) : _owner(owner) { }

        ObjectGuid GetOwner() const { return _owner; }
        uint8 GetMaxLegionnaires() const;                                 // total command slots (base + ec/ec2/lc)
        uint8 LegionnaireCap() const;                                     // slots left for legionnaires (minus any active greater demon)
        uint8 GreaterDemonSlots() const { return _greaterDemonSlots; }    // slots the active greater demon occupies (0 = none)
        ObjectGuid GreaterDemonGuid() const { return _greaterDemonGuid; } // the active greater demon (empty if none)
        uint32 Count() const { return uint32(_legionnaires.size()); }
        std::vector<ObjectGuid> const& Legionnaires() const { return _legionnaires; }

        // Reserve slotCost command slots for a greater demon (Infernal/Doomguard),
        // despawning any existing greater demon and evicting the oldest legionnaires to
        // fit. Caller ensures GetMaxLegionnaires() >= slotCost (checked at CheckCast).
        void RegisterGreaterDemon(Player* owner, Creature* demon, uint8 slotCost);

        // Adds a legionnaire; if at capacity, unsummons the OLDEST first (design §2).
        void Add(Creature* legionnaire);
        void Remove(ObjectGuid guid);
        void DismissAll();

        // Summon a legionnaire (owned, mirroring AI, spread spawn), evicting the
        // oldest if full. Shared by `.legion recruit` and login restore.
        Creature* Recruit(Player* owner, uint32 entry, float healthPct = 1.0f);

        // Persistence (character_legion_slots).
        void Save() const;
        void QueueRestore(uint32 entry, float healthPct);                  // applied ~2s after login

        void Update(uint32 diff);                                          // batched mirroring tick
        void QueueReconcile() { _reconcileTalents = true; }                // re-sync Felguard spells next tick (after a respec)

        // Demon-death consumers (§8.2). OnDemonDeath handles Demonic Rebirth (queue an
        // instant resummon, ICD-gated) + Bound by Blood (survivor buff + shard refund).
        void OnDemonDeath(Player* owner, Creature* dead);

        // Bound by Blood transient survivor buff, read by the damage hook (LegionBuffDamage)
        // and by PetScaling attack-speed (LegionBuffHaste). Returns 0 once the window lapses.
        void TriggerLegionBuff(float dmgFrac, float hasteFrac, uint32 durMs);
        float LegionBuffDamage() const;
        float LegionBuffHaste() const;

        // Demons you actively command right now: anchor pet + legionnaires + greater demon.
        uint8 CommandedDemonCount(Player* owner) const;

        uint32 RebirthReadyInMs() const;        // ms until Demonic Rebirth can proc again (0 = ready)

        // Eternal Servitude REMOVES the vanilla Inferno/Ritual of Doom (which are quest-
        // learned) and REMEMBERS them, so a respec restores only what we took — never
        // granting them to a warlock who never had them.
        void SyncVanillaGreaterDemons(Player* owner, bool eternal);

    private:
        void Mirror(Player* owner);
        void DoRestore(Player* owner);
        void OnPoolChanged();

        struct PendingDemon { uint32 entry; float healthPct; };

        ObjectGuid _owner;
        std::vector<ObjectGuid> _legionnaires;                             // oldest -> newest
        uint32 _mirrorTimer = 0;
        int32 _lastAppliedSP = -1;                                         // owner SP at last inheritance re-sync
        ObjectGuid _followTargetGuid;                                      // last follow target (re-issue on change)
        size_t _lastFormationCount = 0;                                    // roster size at last formation re-fan
        bool _reconcileTalents = false;                                    // deferred Felguard-spell re-sync (respec)
        ObjectGuid _greaterDemonGuid;                                      // active permanent/temp greater demon (Infernal/Doomguard)
        uint8 _greaterDemonSlots = 0;                                      // command slots it occupies (0 = none out)
        std::vector<uint32> _esRemoved;                                    // vanilla summons ES stripped (to restore on respec)
        bool _esRemovedLoaded = false;                                     // _esRemoved lazy-loaded from DB yet?
        std::vector<PendingDemon> _restore;                                // queued login restore
        uint32 _restoreTimer = 0;

        // Bound by Blood survivor buff (transient; getMSTime-based expiry).
        float _legionBuffDmg = 0.0f;
        float _legionBuffHaste = 0.0f;
        uint32 _legionBuffUntilMs = 0;

        // Demonic Rebirth: instant-resummon queue (entries) + shared ICD.
        std::vector<uint32> _pendingRebirth;
        uint32 _rebirthReadyAtMs = 0;
        bool _lastLegionBuffActive = false;                                // for one refresh after a bbb window lapses
    };

    class CommandPoolMgr
    {
    public:
        static CommandPoolMgr* instance();

        CommandPool* Find(ObjectGuid player);
        CommandPool& GetOrCreate(ObjectGuid player);
        void Remove(ObjectGuid player);                                    // dismisses + drops the pool
        void Update(uint32 diff);

    private:
        std::unordered_map<ObjectGuid, CommandPool> _pools;
    };
}

#define sCommandPoolMgr Demonology::CommandPoolMgr::instance()

#endif // MOD_DEMONOLOGY_REWORK_COMMAND_POOL_H
