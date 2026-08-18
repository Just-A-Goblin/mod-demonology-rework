/*
 * mod-demonology-rework — DemonAI (Phase 1).
 *
 * A minimal guardian AI attached at runtime (Creature::AIM_Initialize) to demons
 * we command directly (Wild Imps, recruited legionnaires). It:
 *  - attacks its ASSIGNED target (hostile OR neutral) and NEVER evades to spawn,
 *  - never auto-aggros on sight (no self-fighting / no wandering onto neutrals),
 *  - optionally (autoAssist) re-engages the owner's target once its own dies,
 *  - optionally (castSpellId) behaves as a ranged caster: holds at cast range and
 *    casts the given spell on cooldown instead of meleeing (Wild Imp Firebolt).
 *
 * Why: a creature's default combat AI is threat-gated and evades a non-hostile
 * (neutral) target before landing the hit that would flip it hostile, and it
 * auto-aggros by react state. Real pets avoid this via PetAI (needs CharmInfo,
 * guardians lack). This replicates the parts we need.
 */
#ifndef MOD_DEMONOLOGY_REWORK_DEMON_AI_H
#define MOD_DEMONOLOGY_REWORK_DEMON_AI_H

#include "CommandPool.h"
#include "Define.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"

namespace Demonology
{
    struct GuardianAttackerAI : public ScriptedAI
    {
        explicit GuardianAttackerAI(Creature* creature, bool autoAssist = false,
                                    uint32 castSpellId = 0, uint32 castCooldownMs = 2000)
            : ScriptedAI(creature), _autoAssist(autoAssist),
              _castSpellId(castSpellId), _castCooldownMs(castCooldownMs) { }

        void EnterEvadeMode(EvadeReason /*why*/) override { }   // never reset to spawn
        void MoveInLineOfSight(Unit* /*who*/) override { }       // never auto-aggro (no self-fight)

        // Real death of an owned demon (§8.2). Dismiss/stash/logout go through
        // DespawnOrUnsummon and never reach here, so this fires on genuine deaths only —
        // driving Demonic Rebirth + Bound by Blood via the owner's Command Pool.
        void JustDied(Unit* /*killer*/) override
        {
            if (Unit* owner = me->GetOwner())
                if (Player* p = owner->ToPlayer())
                    Demonology::NotifyDemonDeath(p, me);
        }

        // Casters engage without forcing a melee chase: hold at cast range and let
        // UpdateAI drive the casting. Melee demons keep the default chase.
        void AttackStart(Unit* who) override
        {
            if (!who)
                return;
            if (_castSpellId)
            {
                if (me->Attack(who, /*meleeAttack=*/false))
                    me->GetMotionMaster()->MoveChase(who, CAST_RANGE);
            }
            else
                ScriptedAI::AttackStart(who);
        }

        void UpdateAI(uint32 diff) override
        {
            // autoAssist demons TRACK the owner's current target, so they follow the
            // owner's target switches (like the legion mirror) instead of sticking to
            // their first victim until it dies — a fast meleer looks like it switches
            // (its target dies quickly) while a slow caster never would. Non-autoAssist
            // demons (recruited legionnaires) are driven externally by the Command Pool.
            Unit* target = me->GetVictim();
            if (_autoAssist)
                if (Unit* owner = me->GetOwner())
                {
                    // Assist: attack whatever the owner is attacking.
                    if (Unit* ov = owner->GetVictim())
                        if (ov->IsAlive() && !me->IsFriendlyTo(ov))
                            target = ov;

                    // Defensive retaliation: if not already engaged, rally to ANY attack
                    // on the owner or any of their demons — so a lone greater demon / imp
                    // responds when the owner, the pet, or a legionnaire is attacked, not
                    // only when the owner attacks first. Shared with the legionnaire mirror
                    // (Demonology::FindLegionThreat) so the whole army reacts as one.
                    if (!target || !target->IsAlive() || me->IsFriendlyTo(target))
                        if (Player* p = owner->ToPlayer())
                            if (Unit* threat = Demonology::FindLegionThreat(p))
                                target = threat;
                }

            if (target && target->IsAlive() && !me->IsFriendlyTo(target))
            {
                if (me->GetVictim() != target)
                {
                    if (_castSpellId && me->HasUnitState(UNIT_STATE_CASTING))
                        me->InterruptNonMeleeSpells(false);   // stop casting at the old target
                    AttackStart(target);
                }

                if (_castSpellId)
                {
                    // Ranged caster: wait out the current cast, then fire on cooldown.
                    if (me->HasUnitState(UNIT_STATE_CASTING))
                        return;
                    _castTimer = (_castTimer > diff) ? _castTimer - diff : 0;
                    if (_castTimer == 0 && me->IsWithinLOSInMap(target))
                    {
                        me->CastSpell(target, _castSpellId, false);
                        _castTimer = _castCooldownMs;
                    }
                    return;                     // never melee — casters cast
                }
                DoMeleeAttackIfReady();
                return;
            }

            // Nothing valid to fight: drop the dead/gone victim.
            if (me->GetVictim())
                me->AttackStop();

            if (!_autoAssist)
                return;

            // Idle: follow the owner instead of loitering on the corpse.
            if (Unit* owner = me->GetOwner())
                if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                    me->GetMotionMaster()->MoveFollow(owner, 2.0f, me->GetFollowAngle());
        }

    private:
        static constexpr float CAST_RANGE = 25.0f;   // hold distance for ranged casters (< spell range)

        bool _autoAssist;
        uint32 _castSpellId;
        uint32 _castCooldownMs;
        uint32 _castTimer = 0;                        // 0 == ready to cast
    };
}

#endif // MOD_DEMONOLOGY_REWORK_DEMON_AI_H
