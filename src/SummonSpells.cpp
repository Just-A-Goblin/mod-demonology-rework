/*
 * mod-demonology-rework — Summon Wild Imps (PLAN §5).
 *
 * DBC spell 290001 is a dummy shell; this SpellScript summons the imps in code
 * so the generation cap (Wrath of the Legion) can be enforced here later. Slice
 * scope: summon N Wild Imps as owned guardians for the configured duration and
 * send them at the caster's current target.
 */
#include "CommandPool.h"
#include "DemonAI.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "PetScaling.h"

#include "Creature.h"
#include "CreatureAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "SpellScript.h"
#include "TemporarySummon.h"

#include <algorithm>
#include <cmath>
#include <list>

using namespace Demonology;

class spell_demonology_summon_wild_imps : public SpellScript
{
    PrepareSpellScript(spell_demonology_summon_wild_imps);

    // Path B: Summon Wild Imps costs Soul Shards (Improved Legion trims the cost).
    // Block the cast up-front if the player can't afford it, so no shard is wasted.
    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_FAILED_ERROR;
        if (caster->GetItemCount(ITEM_SOUL_SHARD) < Demonology::WildImpShardCost(caster))
            return SPELL_FAILED_REAGENTS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;

        // The spell requires an enemy target (unit_target_enemy) with a real range,
        // so there is always an explicit target here.
        Unit* target = GetExplTargetUnit();
        if (!target || !target->IsAlive())
            return;

        // Engage the target so the whole legion responds (anchor + legionnaires
        // mirror the owner's victim) and a neutral target flips hostile.
        caster->Attack(target, true);

        // Charge the shards now that the summon is going through (CheckCast already
        // verified affordability; deduct once, here, not per imp).
        if (uint32 const cost = Demonology::WildImpShardCost(caster))
            caster->DestroyItemCount(ITEM_SOUL_SHARD, cost, true);

        uint32 const count = gConfig.WildImpCount;

        for (uint32 i = 0; i < count; ++i)
        {
            float const angle = caster->GetOrientation() + (float(i) / std::max<uint32>(count, 1)) * 2.0f * float(M_PI);
            float const dist = 2.5f;
            float const x = caster->GetPositionX() + std::cos(angle) * dist;
            float const y = caster->GetPositionY() + std::sin(angle) * dist;
            float const z = caster->GetPositionZ();

            TempSummon* imp = caster->SummonCreature(NPC_WILD_IMP, x, y, z, caster->GetOrientation(),
                TEMPSUMMON_TIMED_DESPAWN, gConfig.WildImpDurationMs);
            if (!imp)
                continue;

            imp->SetOwnerGUID(caster->GetGUID());
            imp->SetCreatorGUID(caster->GetGUID());   // client attributes its damage to the owner
            imp->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);   // shows in the owner's combat log / floating text
            imp->SetFaction(caster->GetFaction());
            imp->SetLevel(caster->GetLevel());
            imp->SetReactState(REACT_PASSIVE);   // no auto-aggro; the AI drives targeting

            // Stat inheritance (Phase 6): scale health/melee off the owner. Firebolt
            // damage itself scales live at cast time (see the Firebolt SpellScript).
            PetScaling::ApplyInheritance(caster, imp);

            // No-evade guardian AI, autoAssist + ranged caster: holds at cast range
            // and casts Firebolt (290900) on the target (hostile OR neutral), and
            // once its target dies re-engages whatever the owner is fighting.
            imp->AIM_Initialize(new Demonology::GuardianAttackerAI(
                imp, /*autoAssist=*/true, SPELL_WILD_IMP_FIREBOLT, /*castCooldownMs=*/2000));
            if (imp->IsAlive() && imp->AI())
                imp->AI()->AttackStart(target);
        }
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_summon_wild_imps::CheckCast);
        OnEffectHitTarget += SpellEffectFn(spell_demonology_summon_wild_imps::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Wild Imp Firebolt (290900) — the pet nuke. Its cloned base damage is trivial;
// the pack's power comes from owner spell power. Since a plain creature gets no SP
// from the core (SpellBaseDamageBonusDone only credits IsPlayer casters), we add
// owner_SP * coefficient here, live at cast time so it always reflects current gear
// and procs (design §9.4 — Wild Imp Firebolt ~28% SP/bolt).
class spell_demonology_wild_imp_firebolt : public SpellScript
{
    PrepareSpellScript(spell_demonology_wild_imp_firebolt);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;
        Player* owner = caster->GetOwner() ? caster->GetOwner()->ToPlayer() : nullptr;
        if (!owner)
            return;

        // Base SP contribution; Vicious Pact / Fel Armory scale the whole hit in the
        // demonology_demon_damage UnitScript (uniform for pet + legionnaires + imps).
        int32 const bonus = int32(float(PetScaling::OwnerSpellPower(owner)) * gConfig.WildImpSPCoefficient);
        if (bonus > 0)
            SetHitDamage(GetHitDamage() + bonus);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_wild_imp_firebolt::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Per-type legionnaire summons (Summon Imp/Felguard/Succubus/Felhound Legionnaire).
// One script bound to all four spells: it maps the cast spell to a base creature
// entry and adds a guardian to the Command Pool via Recruit (which evicts the oldest
// when at the slot cap). Charges Soul Shards, gated at CheckCast so the cast is
// blocked (not just refunded) when the player is short. See summon-roster-design.
class spell_demonology_summon_legionnaire : public SpellScript
{
    PrepareSpellScript(spell_demonology_summon_legionnaire);

    static uint32 EntryForSpell(uint32 spellId)
    {
        switch (spellId)
        {
            case SPELL_SUMMON_LEGIONNAIRE_IMP:       return NPC_BASE_IMP;
            case SPELL_SUMMON_LEGIONNAIRE_FELGUARD:  return NPC_BASE_FELGUARD;
            case SPELL_SUMMON_LEGIONNAIRE_SUCCUBUS:  return NPC_BASE_SUCCUBUS;
            case SPELL_SUMMON_LEGIONNAIRE_FELHOUND:  return NPC_BASE_FELHUNTER;
            case SPELL_SUMMON_LEGIONNAIRE_VOIDWALKER: return NPC_BASE_VOIDWALKER;
            default:                                 return 0;
        }
    }

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_FAILED_ERROR;
        // Block only when the player has NO command slots at all (no Expanded Command). If a
        // greater demon is holding the slots, the summon is allowed — it evicts the greater
        // demon to make room (handled in HandleDummy), rather than being blocked.
        if (sCommandPoolMgr->GetOrCreate(caster->GetGUID()).GetMaxLegionnaires() == 0)
        {
            ChatHandler(caster->GetSession()).SendSysMessage("You have no legion command slots. Train Expanded Command.");
            return SPELL_FAILED_DONT_REPORT;
        }
        // The Felguard is gated behind the Summon Felguard talent (pet AND legionnaire).
        if (GetSpellInfo()->Id == SPELL_SUMMON_LEGIONNAIRE_FELGUARD &&
            !caster->HasTalent(SPELL_TALENT_SUMMON_FELGUARD, caster->GetActiveSpec()))
        {
            ChatHandler(caster->GetSession()).SendSysMessage("You must train Summon Felguard first.");
            return SPELL_FAILED_DONT_REPORT;
        }
        if (caster->GetItemCount(ITEM_SOUL_SHARD) < gConfig.SummonLegionnaireShardCost)
            return SPELL_FAILED_REAGENTS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;

        uint32 const entry = EntryForSpell(GetSpellInfo()->Id);
        if (!entry)
            return;

        Demonology::CommandPool& pool = sCommandPoolMgr->GetOrCreate(caster->GetGUID());

        // Summoning a legionnaire evicts any active greater demon (Infernal/Doomguard) and
        // frees ALL its command slots — the greater demon is mutually exclusive with growing
        // the legion, so it's always the first casualty (per playtest).
        pool.DespawnGreaterDemon(caster);

        if (pool.Recruit(caster, entry))                        // spawn + inherit + add (evicts oldest if full)
            if (gConfig.SummonLegionnaireShardCost > 0)
                caster->DestroyItemCount(ITEM_SOUL_SHARD, gConfig.SummonLegionnaireShardCost, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_summon_legionnaire::CheckCast);
        OnEffectHit += SpellEffectFn(spell_demonology_summon_legionnaire::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Greater demons (Summon Infernal / Summon Doomguard) — big-cooldown TEMPORARY
// guardians that do NOT occupy a legion slot (Eternal Servitude makes them
// permanent later). They engage the owner's target and despawn after a duration.
// Infernal melees; Doomguard is a ranged Doom Bolt caster. One script, both spells.
class spell_demonology_summon_greater_demon : public SpellScript
{
    PrepareSpellScript(spell_demonology_summon_greater_demon);

    // These are our PERMANENT greater-demon summons, unlocked by Eternal Servitude (a deep
    // Command talent, so ES implies the full slot spine). Without ES a warlock uses the
    // vanilla Inferno / Ritual of Doom (temporary) instead. Belt-and-suspenders — the
    // spells are only granted (hybrid-learn) when ES is trained.
    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_FAILED_ERROR;
        if (!caster->HasTalent(SPELL_TALENT_ETERNAL_SERVITUDE, caster->GetActiveSpec()))
        {
            ChatHandler(caster->GetSession()).SendSysMessage("Requires the Eternal Servitude talent.");
            return SPELL_FAILED_DONT_REPORT;
        }
        return SPELL_CAST_OK;
    }

    void HandleSummon(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;
        uint32 const entry = (GetSpellInfo()->Id == SPELL_SUMMON_DOOMGUARD) ? NPC_BASE_DOOMGUARD : NPC_BASE_INFERNAL;
        Demonology::SummonGreaterDemon(caster, entry);     // shared with the cross-map restore
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_summon_greater_demon::CheckCast);
        OnEffectHit += SpellEffectFn(spell_demonology_summon_greater_demon::HandleSummon, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// Shared greater-demon summon (used by the spell above + the cross-map teleport restore in
// CommandPool). Full setup: owner attribution, stat inheritance, command-slot reservation,
// AI (Doomguard = ranged Doom Bolt caster, Infernal = melee), engage-or-follow.
Creature* Demonology::SummonGreaterDemon(Player* owner, uint32 entry)
{
    if (!owner)
        return nullptr;

    bool const isDoomguard = (entry == NPC_BASE_DOOMGUARD);
    uint32 const durationMs = isDoomguard ? gConfig.DoomguardDurationMs : gConfig.InfernalDurationMs;
    uint8 const  slotCost   = isDoomguard ? gConfig.DoomguardCommandSlots : gConfig.InfernalCommandSlots;

    // ES-gated in practice, so permanent when Eternal Servitude is trained; else timed.
    bool const permanent = owner->HasTalent(SPELL_TALENT_ETERNAL_SERVITUDE, owner->GetActiveSpec());

    float const angle = owner->GetOrientation();
    float const dist  = 3.0f;
    float const x = owner->GetPositionX() + std::cos(angle) * dist;
    float const y = owner->GetPositionY() + std::sin(angle) * dist;

    TempSummon* demon = owner->SummonCreature(entry, x, y, owner->GetPositionZ(), owner->GetOrientation(),
        permanent ? TEMPSUMMON_MANUAL_DESPAWN : TEMPSUMMON_TIMED_DESPAWN, permanent ? 0 : durationMs);
    if (!demon)
        return nullptr;

    demon->SetOwnerGUID(owner->GetGUID());
    demon->SetCreatorGUID(owner->GetGUID());               // client attributes its damage to the owner
    demon->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
    demon->SetFaction(owner->GetFaction());
    demon->SetLevel(owner->GetLevel());
    demon->SetReactState(REACT_DEFENSIVE);

    PetScaling::ApplyInheritance(owner, demon);             // scale health + melee off owner SP

    // Reserve its command slots (Infernal 2 / Doomguard 3): despawns any prior greater demon
    // and evicts the oldest legionnaires to fit, so it can't stack on a full legion.
    sCommandPoolMgr->GetOrCreate(owner->GetGUID()).RegisterGreaterDemon(owner, demon, slotCost);

    if (!isDoomguard && gConfig.InfernalScale > 0.0f)       // the Infernal model is oversized
        demon->SetObjectScale(gConfig.InfernalScale);

    if (isDoomguard)
        demon->AIM_Initialize(new Demonology::GuardianAttackerAI(demon, /*autoAssist=*/true, SPELL_DOOM_BOLT, /*castCooldownMs=*/3000));
    else
        demon->AIM_Initialize(new Demonology::GuardianAttackerAI(demon, /*autoAssist=*/true));

    // Engage the owner's current foe immediately; else follow so it isn't left idle.
    Unit* target = owner->GetVictim();
    if (!target)
        target = owner->GetSelectedUnit();

    if (target && target->IsAlive() && demon->IsAlive() && demon->AI() && !demon->IsFriendlyTo(target))
        demon->AI()->AttackStart(target);
    else
        demon->GetMotionMaster()->MoveFollow(owner, 2.0f, demon->GetFollowAngle());

    return demon;
}

// Doom Bolt (290901) — the Doomguard's nuke. Same live SP-inheritance as Wild Imp
// Firebolt: adds owner_SP * Doomguard.DoomBoltSPCoefficient to the hit (PLAN §8).
class spell_demonology_doom_bolt : public SpellScript
{
    PrepareSpellScript(spell_demonology_doom_bolt);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;
        Player* owner = caster->GetOwner() ? caster->GetOwner()->ToPlayer() : nullptr;
        if (!owner)
            return;

        // Doom Bolt was cloned from 40876, which carries a ~2249 flat base — far too high
        // (the "hits crazy hard"). OVERRIDE the whole hit with a controlled base + SP
        // scaling; Vicious Pact / Fel Armory still multiply it in the demon_damage UnitScript.
        int32 const dmg = int32(gConfig.DoomBoltBaseDamage)
            + int32(float(PetScaling::OwnerSpellPower(owner)) * gConfig.DoomBoltSPCoefficient);
        SetHitDamage(std::max<int32>(1, dmg));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_doom_bolt::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Doom Blast (core spell 40878) — the AOE second hit Doom Bolt triggers. Its stock damage
// (~1.5k) two-shot mobs, so when it comes from OUR Doomguard we override it with a
// controlled base + SP scaling (bound to 40878 via spell_script_names; scoped to a
// warlock-owned caster so it never touches other sources of this spell).
class spell_demonology_doom_blast : public SpellScript
{
    PrepareSpellScript(spell_demonology_doom_blast);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Player* owner = caster ? Demonology::WarlockOwnerOfDemon(caster) : nullptr;
        if (!owner)
            return;                                     // not our Doomguard — leave stock behaviour
        int32 const dmg = int32(gConfig.DoomBlastBaseDamage)
            + int32(float(PetScaling::OwnerSpellPower(owner)) * gConfig.DoomBlastSPCoefficient);
        SetHitDamage(std::max<int32>(1, dmg));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_doom_blast::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Gate on the base Summon Felguard PET cast (core spell 30146): block it unless the
// player has trained the Summon Felguard talent. Bound via spell_script_names so it
// runs alongside the core summon effect and aborts the cast at CheckCast. The matching
// Felguard LEGIONNAIRE (290003) is gated in spell_demonology_summon_legionnaire above.
class spell_demonology_gate_summon_felguard : public SpellScript
{
    PrepareSpellScript(spell_demonology_gate_summon_felguard);

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_CAST_OK;
        if (!caster->HasTalent(SPELL_TALENT_SUMMON_FELGUARD, caster->GetActiveSpec()))
        {
            ChatHandler(caster->GetSession()).SendSysMessage("You must train Summon Felguard first.");
            return SPELL_FAILED_DONT_REPORT;
        }
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_gate_summon_felguard::CheckCast);
    }
};

// Suppress the vanilla temporary greater-demon summons (Inferno 1122 / Ritual of Doom
// 18540) once the warlock has Eternal Servitude — otherwise a permanent Infernal could
// be paired with a fresh temporary one. Bound to both spells via spell_script_names.
class spell_demonology_suppress_vanilla_greater_demon : public SpellScript
{
    PrepareSpellScript(spell_demonology_suppress_vanilla_greater_demon);

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (caster && caster->HasTalent(SPELL_TALENT_ETERNAL_SERVITUDE, caster->GetActiveSpec()))
        {
            ChatHandler(caster->GetSession()).SendSysMessage("Eternal Servitude replaces this — summon your permanent Infernal/Doomguard instead.");
            return SPELL_FAILED_DONT_REPORT;
        }
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_suppress_vanilla_greater_demon::CheckCast);
    }
};

void AddSC_demonology_summon_spells()
{
    RegisterSpellScript(spell_demonology_summon_wild_imps);
    RegisterSpellScript(spell_demonology_wild_imp_firebolt);
    RegisterSpellScript(spell_demonology_summon_legionnaire);
    RegisterSpellScript(spell_demonology_summon_greater_demon);
    RegisterSpellScript(spell_demonology_doom_bolt);
    RegisterSpellScript(spell_demonology_doom_blast);
    RegisterSpellScript(spell_demonology_gate_summon_felguard);
    RegisterSpellScript(spell_demonology_suppress_vanilla_greater_demon);
}
