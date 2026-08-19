/*
 * mod-demonology-rework — Command Demon (DESIGN_V2 §5).
 *
 * One button (290013): the ANCHOR (slot-0 pet) fires a signature, every legionnaire of a
 * type fires a free echo, and a greater demon adds a response. All targeting flows from the
 * anchor's current victim. This file holds the startup rank resolver, the dispatcher (with
 * the five signatures / five echoes / two responses), and the SpellScripts.
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "PetScaling.h"

#include "Cell.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "ThreatManager.h"
#include "Unit.h"

#include <string>
#include <unordered_map>
#include <vector>

using namespace Demonology;

namespace
{
    // Top ranks of the vanilla abilities the dispatcher casts, resolved from spell_dbc at
    // startup off the rank-1 anchors in DemonologyIds.h (never hardcode top-rank IDs, §5.7.3).
    struct VanillaTopRanks
    {
        uint32 intercept = SPELL_VANILLA_INTERCEPT_R1;
        uint32 cleave    = SPELL_VANILLA_CLEAVE_R1;
        uint32 spellLock = SPELL_VANILLA_SPELL_LOCK_R1;
        uint32 suffering = SPELL_VANILLA_SUFFERING_R1;
        uint32 firebolt  = SPELL_VANILLA_IMP_FIREBOLT_R1;
    };
    VanillaTopRanks gTop;

    // Last-press human summary per owner, for `.legion dumpstats`.
    std::unordered_map<ObjectGuid, std::string> g_lastPress;

    constexpr TriggerCastFlags TRIG = TRIGGERED_FULL_MASK;

    // --- Devour Magic helper: remove the first DISPEL_MAGIC aura of the wanted polarity. ---
    bool RemoveFirstMagicAura(Unit* u, bool wantPositive)
    {
        if (!u)
            return false;
        for (auto const& pair : u->GetAppliedAuras())
        {
            AuraApplication* app = pair.second;
            if (!app)
                continue;
            SpellInfo const* si = app->GetBase()->GetSpellInfo();
            if (si->Dispel != DISPEL_MAGIC)
                continue;
            if (app->IsPositive() != wantPositive)
                continue;
            u->RemoveAura(app);                 // one only — return immediately (no more iteration)
            return true;
        }
        return false;
    }

    // Enthralling Presence (Succubus signature): apply the debuff (290501) to every enemy
    // within radius of the anchor's target, and imperatively drop the owner's + every pooled
    // demon's threat on each (§5.5). Skips units the owner has charmed (manual Seduction).
    uint32 EnthrallingPresence(Player* owner, Unit* anchor, Unit* target, CommandPool* pool)
    {
        if (!owner || !anchor || !target)
            return 0;

        std::list<Unit*> enemies;
        Acore::AnyUnfriendlyUnitInObjectRangeCheck check(target, owner, gConfig.EnthrallingPresenceRadius);
        Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(target, enemies, check);
        Cell::VisitObjects(target, searcher, gConfig.EnthrallingPresenceRadius);
        enemies.push_back(target);              // always include the primary target

        // The units whose threat we drop: owner + anchor + legionnaires + greater demon.
        std::vector<Unit*> mine;
        mine.push_back(owner);
        mine.push_back(anchor);
        if (pool)
        {
            for (ObjectGuid guid : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
                    mine.push_back(c);
            if (Creature* gd = ObjectAccessor::GetCreature(*owner, pool->GreaterDemonGuid()))
                mine.push_back(gd);
        }

        uint32 affected = 0;
        for (Unit* e : enemies)
        {
            if (!e || !e->IsAlive())
                continue;
            if (e->GetCharmerGUID() == owner->GetGUID())
                continue;                       // never touch a unit the owner has Seduced/charmed
            anchor->CastSpell(e, SPELL_ENTHRALLING_PRESENCE, TRIG);
            for (Unit* u : mine)
                if (u)
                    e->GetThreatMgr().ModifyThreatByPercent(u, -gConfig.EnthrallingThreatDropPct);
            ++affected;
        }
        return affected;
    }

    // --- Anchor signatures (fire once, from the anchor). Returns a summary. ---
    std::string AnchorSignature(Player* owner, Unit* anchor, Unit* target, CommandPool* pool)
    {
        bool const hasTarget = target && target->IsAlive();
        switch (anchor->GetEntry())
        {
            case NPC_BASE_FELGUARD:
                if (!hasTarget) return "Felguard: (no target)";
                anchor->CastSpell(target, gTop.intercept, TRIG);
                anchor->CastSpell(target, gTop.cleave, TRIG);
                return "Felguard: Intercept + Cleave";
            case NPC_BASE_FELHUNTER:
                if (!hasTarget) return "Felhound: (no target)";
                anchor->CastSpell(target, gTop.spellLock, TRIG);
                return "Felhound: Spell Lock";
            case NPC_BASE_VOIDWALKER:                       // no target needed
            {
                anchor->CastSpell(anchor, gTop.suffering, TRIG);   // AoE taunt around the VW
                int32 const absorb = int32(float(owner->GetMaxHealth()) * gConfig.VoidwalkerOwnerAbsorbPctOfHp);
                owner->CastCustomSpell(SPELL_VOIDWALKER_COMMAND_SHIELD, SPELLVALUE_BASE_POINT0, absorb, owner, TRIG);
                return "Voidwalker: Suffering taunt + owner shield " + std::to_string(absorb);
            }
            case NPC_BASE_SUCCUBUS:
            {
                if (!hasTarget) return "Succubus: (no target)";
                uint32 n = EnthrallingPresence(owner, anchor, target, pool);
                return "Succubus: Enthralling Presence on " + std::to_string(n) + " enemy(ies) (-" + std::to_string(gConfig.EnthrallingThreatDropPct) + "% threat)";
            }
            case NPC_BASE_IMP:
                if (!hasTarget) return "Imp: (no target)";
                for (int i = 0; i < 3; ++i)
                    anchor->CastSpell(target, gTop.firebolt, TRIG);
                return "Imp: Firebolt volley x3";
            default:
                return "anchor entry " + std::to_string(anchor->GetEntry()) + ": (no signature)";
        }
    }

    // --- Legionnaire echoes (per own type; free). Returns a short tag or "" if it no-oped. ---
    std::string LegionnaireEcho(Player* owner, Unit* anchor, Creature* leg, Unit* target)
    {
        bool const hasTarget = target && target->IsAlive();
        switch (leg->GetEntry())
        {
            case NPC_BASE_FELGUARD:
                if (!hasTarget) return "";
                leg->CastSpell(target, gTop.cleave, TRIG);
                return "Cleave";
            case NPC_BASE_FELHUNTER:                        // Devour Magic — deterministic, no AI
                if (hasTarget && RemoveFirstMagicAura(target, /*wantPositive=*/true))
                    return "Devour(purge)";                 // purged a buff from the target
                if (RemoveFirstMagicAura(owner, false))     // else cleanse a magic debuff: owner -> anchor -> self
                    return "Devour(cleanse owner)";
                if (anchor && RemoveFirstMagicAura(anchor, false))
                    return "Devour(cleanse anchor)";
                if (RemoveFirstMagicAura(leg, false))
                    return "Devour(cleanse self)";
                return "Devour(nothing to dispel)";
            case NPC_BASE_VOIDWALKER:                       // self-shield — NO target needed
            {
                int32 const absorb = int32(float(leg->GetMaxHealth()) * gConfig.VoidwalkerSelfAbsorbPctOfHp);
                leg->CastCustomSpell(SPELL_VOIDWALKER_CONSUME_SHADOWS, SPELLVALUE_BASE_POINT0, absorb, leg, TRIG);
                return "self-shield " + std::to_string(absorb);
            }
            case NPC_BASE_SUCCUBUS:
                if (!hasTarget) return "";
                leg->CastSpell(target, SPELL_EMPOWERED_LASH_OF_PAIN, TRIG);
                return "Empowered Lash";
            case NPC_BASE_IMP:
                if (!hasTarget) return "";
                leg->CastSpell(target, gTop.firebolt, TRIG);
                leg->CastSpell(target, gTop.firebolt, TRIG);
                return "Firebolt x2";
            default:
                return "";
        }
    }
}

// The dispatcher (§5.7.2). Anchor victim drives targeting. Declared in CommandPool.h so the
// SpellScript and `.legion command` can call it.
void Demonology::CommandDemonPress(Player* owner)
{
    if (!owner)
        return;
    Pet* anchor = owner->GetPet();
    if (!anchor || !anchor->IsAlive())
        return;

    Unit* target = anchor->GetVictim();
    CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID());

    std::string const sig = AnchorSignature(owner, anchor, target, pool);

    // Echoes fire for every legionnaire (NOT gated on a target — the Voidwalker self-shield
    // and Devour Magic's self-cleanse need none; the target-requiring echoes no-op cleanly).
    std::string echoLog;
    uint32 echoes = 0;
    if (pool)
        for (ObjectGuid guid : pool->Legionnaires())
            if (Creature* c = ObjectAccessor::GetCreature(*owner, guid))
            {
                std::string const tag = LegionnaireEcho(owner, anchor, c, target);
                if (!tag.empty())
                {
                    echoLog += (echoes ? ", " : "") + tag;
                    ++echoes;
                }
            }
    if (echoLog.empty())
        echoLog = "none";

    std::string resp = "none";
    if (pool)
        if (Creature* gd = ObjectAccessor::GetCreature(*owner, pool->GreaterDemonGuid()))
        {
            if (gd->GetEntry() == NPC_BASE_INFERNAL)
            {
                gd->CastSpell(gd, SPELL_INFERNAL_COMMAND_PULSE, TRIG);
                resp = "Infernal Command Pulse";
            }
            else if (gd->GetEntry() == NPC_BASE_DOOMGUARD && target)
            {
                gd->CastSpell(target, SPELL_DOOM_BOLT, TRIG);
                resp = "Doomguard Doom Bolt";
            }
        }

    // Dark Command (dc): a short haste buff on the demons that just responded.
    if (pool && TalentRank(owner, SPELL_TALENT_DARK_COMMAND, 3))
        pool->TriggerLegionBuff(0.0f, gConfig.DarkCommandHastePct, gConfig.DarkCommandHasteDurationMs);

    g_lastPress[owner->GetGUID()] =
        std::string("anchor[") + std::to_string(anchor->GetEntry()) + "] " + sig
        + " | target:" + (target ? "yes" : "NONE")
        + " | echoes(" + std::to_string(echoes) + "): " + echoLog
        + " | greater: " + resp;
}

std::string Demonology::GetLastCommandPress(ObjectGuid owner)
{
    auto it = g_lastPress.find(owner);
    return it != g_lastPress.end() ? it->second : "(none this session)";
}

// -------------------------------------------------------------------- SpellScripts

class spell_demonology_command_demon : public SpellScript
{
    PrepareSpellScript(spell_demonology_command_demon);

    SpellCastResult CheckCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return SPELL_FAILED_ERROR;

        Pet* anchor = caster->GetPet();
        if (!anchor || !anchor->IsAlive())
        {
            ChatHandler(caster->GetSession()).SendSysMessage("Command Demon needs a living anchor demon.");
            return SPELL_FAILED_DONT_REPORT;
        }
        if (anchor->HasUnitState(UNIT_STATE_STUNNED) || anchor->HasAuraType(SPELL_AURA_MOD_FEAR) || anchor->HasAuraType(SPELL_AURA_MOD_CONFUSE))
        {
            ChatHandler(caster->GetSession()).SendSysMessage("Your anchor demon cannot act right now.");
            return SPELL_FAILED_DONT_REPORT;
        }
        // Every anchor except the Voidwalker needs a hostile target (the anchor's victim).
        if (anchor->GetEntry() != NPC_BASE_VOIDWALKER)
        {
            Unit* v = anchor->GetVictim();
            if (!v || !v->IsAlive() || anchor->IsFriendlyTo(v))
            {
                ChatHandler(caster->GetSession()).SendSysMessage("Your anchor demon needs a hostile target.");
                return SPELL_FAILED_DONT_REPORT;
            }
        }
        if (caster->GetItemCount(ITEM_SOUL_SHARD) < gConfig.CommandDemonShardCost)
            return SPELL_FAILED_REAGENTS;
        return SPELL_CAST_OK;
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;
        if (gConfig.CommandDemonShardCost)
            caster->DestroyItemCount(ITEM_SOUL_SHARD, gConfig.CommandDemonShardCost, true);
        Demonology::CommandDemonPress(caster);
    }

    // Dark Command trims the cooldown after it's been applied (§4/§6). SendSpellCooldown runs
    // before AfterCast, so the DBC 45s is already down — we subtract dc * per-rank.
    void HandleAfterCast()
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;
        if (uint8 dc = Demonology::TalentRank(caster, SPELL_TALENT_DARK_COMMAND, 3))
            caster->ModifySpellCooldown(SPELL_COMMAND_DEMON, -int32(dc * gConfig.DarkCommandCdReductionMsPerRank));
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_demonology_command_demon::CheckCast);
        OnEffectHit += SpellEffectFn(spell_demonology_command_demon::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        AfterCast += SpellCastFn(spell_demonology_command_demon::HandleAfterCast);
    }
};

// Empowered Lash of Pain (290502) — succubus echo: base Lash of Pain + live owner-SP scaling.
class spell_demonology_empowered_lash : public SpellScript
{
    PrepareSpellScript(spell_demonology_empowered_lash);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Player* owner = caster ? Demonology::WarlockOwnerOfDemon(caster) : nullptr;
        if (!owner)
            return;
        int32 const bonus = int32(float(PetScaling::OwnerSpellPower(owner)) * gConfig.EmpoweredLashSPCoefficient);
        if (bonus > 0)
            SetHitDamage(GetHitDamage() + bonus);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_empowered_lash::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Infernal Command Pulse (290503) — Infernal response: fire AoE + live owner-SP scaling.
class spell_demonology_infernal_command_pulse : public SpellScript
{
    PrepareSpellScript(spell_demonology_infernal_command_pulse);

    void HandleDamage(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Player* owner = caster ? Demonology::WarlockOwnerOfDemon(caster) : nullptr;
        if (!owner)
            return;
        int32 const bonus = int32(float(PetScaling::OwnerSpellPower(owner)) * gConfig.InfernalCommandPulseSPCoefficient);
        if (bonus > 0)
            SetHitDamage(GetHitDamage() + bonus);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_infernal_command_pulse::HandleDamage, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

// Startup: resolve the vanilla top ranks once spell_dbc is loaded.
class demonology_command_worldscript : public WorldScript
{
public:
    demonology_command_worldscript() : WorldScript("demonology_command_worldscript") { }

    void OnStartup() override
    {
        auto top = [](uint32 r1) -> uint32 { uint32 t = sSpellMgr->GetLastSpellInChain(r1); return t ? t : r1; };
        gTop.intercept = top(SPELL_VANILLA_INTERCEPT_R1);
        gTop.cleave    = top(SPELL_VANILLA_CLEAVE_R1);
        gTop.spellLock = top(SPELL_VANILLA_SPELL_LOCK_R1);
        gTop.suffering = top(SPELL_VANILLA_SUFFERING_R1);
        gTop.firebolt  = top(SPELL_VANILLA_IMP_FIREBOLT_R1);
    }
};

void AddSC_demonology_command_demon()
{
    RegisterSpellScript(spell_demonology_command_demon);
    RegisterSpellScript(spell_demonology_empowered_lash);
    RegisterSpellScript(spell_demonology_infernal_command_pulse);
    new demonology_command_worldscript();
}
