/*
 * mod-demonology-rework — `.legion` GM command namespace.
 *
 * The primary debugging surface on a solo realm (PLAN §1 Task 3). Subcommands:
 *   .legion pool          — list current command-pool occupants
 *   .legion summon <entry>— spawn a creature entry next to you (test harness)
 *   .legion shards        — report Soul Shard (item 6265) count in bags
 *   .legion dumpstats      — dump owner + pet stat snapshot (grows in Phase 6)
 *
 * Phase 0: pool/dumpstats are honest stubs until CommandPool (Phase 1) and
 * PetScaling (Phase 6) exist; summon/shards are already functional.
 */
#include "Chat.h"
#include "CommandPool.h"
#include "Creature.h"
#include "DemonAI.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Pet.h"
#include "PetScaling.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <cmath>

using namespace Acore::ChatCommands;

namespace
{
    constexpr uint32 SOUL_SHARD_ITEM = 6265;
}

class legion_commandscript : public CommandScript
{
public:
    legion_commandscript() : CommandScript("legion_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable legionCommandTable =
        {
            { "pool",      HandlePoolCommand,      SEC_GAMEMASTER, Console::No },
            { "recruit",   HandleRecruitCommand,   SEC_GAMEMASTER, Console::No },
            { "dismiss",   HandleDismissCommand,   SEC_GAMEMASTER, Console::No },
            { "summon",    HandleSummonCommand,    SEC_GAMEMASTER, Console::No },
            { "shards",    HandleShardsCommand,    SEC_GAMEMASTER, Console::No },
            { "dumpstats", HandleDumpStatsCommand, SEC_GAMEMASTER, Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "legion", legionCommandTable }
        };

        return commandTable;
    }

    static bool HandlePoolCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (Pet* pet = player->GetPet())
            handler->PSendSysMessage("[legion] anchor (slot 0): {} entry {}", pet->GetName(), pet->GetEntry());
        else
            handler->PSendSysMessage("[legion] anchor (slot 0): none (summon a pet)");

        Demonology::CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID());
        uint32 const count = pool ? pool->Count() : 0;
        uint8 const legCap = pool ? pool->LegionnaireCap() : 0;
        uint8 const gdSlots = pool ? pool->GreaterDemonSlots() : 0;
        handler->PSendSysMessage("[legion] legionnaires: {} / {} (total command slots {}, greater demon uses {})",
            count, legCap, pool ? pool->GetMaxLegionnaires() : 0, gdSlots);
        if (pool)
        {
            uint32 slot = 1;
            for (ObjectGuid guid : pool->Legionnaires())
            {
                Creature* c = ObjectAccessor::GetCreature(*player, guid);
                if (c)
                    handler->PSendSysMessage("  slot {}: {} (entry {}) hp {}/{}",
                        slot, c->GetName(), c->GetEntry(), c->GetHealth(), c->GetMaxHealth());
                else
                    handler->PSendSysMessage("  slot {}: <gone>", slot);
                ++slot;
            }
        }
        return true;
    }

    static bool HandleRecruitCommand(ChatHandler* handler, Optional<uint32> entryArg)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 const entry = entryArg.value_or(17252);   // default: Felguard
        if (!sObjectMgr->GetCreatureTemplate(entry))
        {
            handler->PSendSysMessage("[legion] No creature_template for entry {}.", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        Demonology::CommandPool& pool = sCommandPoolMgr->GetOrCreate(player->GetGUID());
        if (!pool.Recruit(player, entry))   // summon + setup + add (evicts oldest if full)
        {
            handler->PSendSysMessage("[legion] Summon failed for entry {}.", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }
        handler->PSendSysMessage("[legion] Recruited entry {} ({} / {} legionnaires).",
            entry, pool.Count(), pool.GetMaxLegionnaires());
        return true;
    }

    static bool HandleDismissCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (Demonology::CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
            pool->DismissAll();
        handler->PSendSysMessage("[legion] Pool dismissed.");
        return true;
    }

    static bool HandleSummonCommand(ChatHandler* handler, uint32 entry)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sObjectMgr->GetCreatureTemplate(entry))
        {
            handler->PSendSysMessage("[legion] No creature_template for entry {}.", entry);
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (Creature* c = player->SummonCreature(entry,
                player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(),
                player->GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, 60000))
        {
            // Friendly + owned so town guards don't kill it instantly.
            c->SetOwnerGUID(player->GetGUID());
            c->SetFaction(player->GetFaction());
            c->SetReactState(REACT_PASSIVE);
            handler->PSendSysMessage("[legion] Summoned entry {} (guid {}) for 60s.",
                entry, c->GetGUID().ToString());
            return true;
        }

        handler->PSendSysMessage("[legion] Summon failed for entry {}.", entry);
        handler->SetSentErrorMessage(true);
        return false;
    }

    static bool HandleShardsCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        handler->PSendSysMessage("[legion] Soul Shards (item {}): {}.",
            SOUL_SHARD_ITEM, player->GetItemCount(SOUL_SHARD_ITEM));
        return true;
    }

    static bool HandleDumpStatsCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        int32 const sp = Demonology::PetScaling::OwnerSpellPower(player);
        handler->PSendSysMessage("[legion] owner {}  lvl {}  SP {}  shards {}",
            player->GetName(), player->GetLevel(), sp, player->GetItemCount(SOUL_SHARD_ITEM));
        handler->PSendSysMessage("[legion] inherit: demon hp = {:.0f}% owner, melee = {:.2f} * SP, firebolt = {:.2f} * SP",
            Demonology::gConfig.InheritHealthPctOfOwner * 100.0f, Demonology::gConfig.InheritMeleeDamagePerSP, Demonology::gConfig.WildImpSPCoefficient);
        float const meleeMult = Demonology::DemonDamageMult(player, true);
        float const spellMult = Demonology::DemonDamageMult(player, false);
        handler->PSendSysMessage("[legion] talents: fc={} vp={} si={} fa={} (felArmor={}) | dmgMult melee={:.2f} spell={:.2f}",
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_FEL_CONDITIONING, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_VICIOUS_PACT, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_SAVAGE_INSTINCTS, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_FEL_ARMORY, 3)),
            uint32(Demonology::OwnerHasFelArmor(player)),
            meleeMult, spellMult);
        // Live per-cast spell bonus these demons add from owner SP (same as the cast scripts).
        handler->PSendSysMessage("[legion] spell bonus vs SP {}: firebolt +{:.0f}, doombolt +{:.0f}",
            sp, float(sp) * Demonology::gConfig.WildImpSPCoefficient * spellMult,
            float(sp) * Demonology::gConfig.DoomBoltSPCoefficient * spellMult);

        // Effective per-swing melee = the computed min/max damage x the melee mult (Vicious
        // Pact/Fel Armory apply at swing-time in the demon_damage hook, so they aren't in the
        // weapon range). Swing = the real base attack time field (haste-independent GetAttackTime
        // would hide Savage Instincts). These reflect what the demon actually hits for.
        if (Pet* pet = player->GetPet())
            handler->PSendSysMessage("[legion] anchor pet: entry {}  hp {}/{}  swing {:.2f}s  hit {:.0f}-{:.0f}",
                pet->GetEntry(), pet->GetHealth(), pet->GetMaxHealth(),
                pet->GetFloatValue(UNIT_FIELD_BASEATTACKTIME) / 1000.0f,
                pet->GetFloatValue(UNIT_FIELD_MINDAMAGE) * meleeMult, pet->GetFloatValue(UNIT_FIELD_MAXDAMAGE) * meleeMult);
        else
            handler->PSendSysMessage("[legion] anchor pet: none");

        // Greater demon (Infernal melee / Doomguard caster), if one is out.
        if (Demonology::CommandPool* gp = sCommandPoolMgr->Find(player->GetGUID()))
            if (Creature* gd = ObjectAccessor::GetCreature(*player, gp->GreaterDemonGuid()))
            {
                if (gd->GetEntry() == Demonology::NPC_BASE_DOOMGUARD)
                    handler->PSendSysMessage("[legion] greater demon: {} (entry {})  hp {}/{}  Doom Bolt ~{:.0f}  Doom Blast(AOE) ~{:.0f}  uses {} slots  [SP {}]",
                        gd->GetName(), gd->GetEntry(), gd->GetHealth(), gd->GetMaxHealth(),
                        (float(Demonology::gConfig.DoomBoltBaseDamage) + float(sp) * Demonology::gConfig.DoomBoltSPCoefficient) * spellMult,
                        (float(Demonology::gConfig.DoomBlastBaseDamage) + float(sp) * Demonology::gConfig.DoomBlastSPCoefficient) * spellMult,
                        uint32(gp->GreaterDemonSlots()), sp);
                else
                    handler->PSendSysMessage("[legion] greater demon: {} (entry {})  hp {}/{}  hit {:.0f}-{:.0f}  uses {} slots",
                        gd->GetName(), gd->GetEntry(), gd->GetHealth(), gd->GetMaxHealth(),
                        gd->GetFloatValue(UNIT_FIELD_MINDAMAGE) * meleeMult, gd->GetFloatValue(UNIT_FIELD_MAXDAMAGE) * meleeMult,
                        uint32(gp->GreaterDemonSlots()));
            }

        // Per-legionnaire: effective per-swing hit (x melee mult) and real swing interval.
        if (Demonology::CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
        {
            uint32 slot = 1;
            for (ObjectGuid guid : pool->Legionnaires())
            {
                if (Creature* c = ObjectAccessor::GetCreature(*player, guid))
                {
                    float const dmgMin = c->GetFloatValue(UNIT_FIELD_MINDAMAGE) * meleeMult;
                    float const dmgMax = c->GetFloatValue(UNIT_FIELD_MAXDAMAGE) * meleeMult;
                    float const swing  = c->GetFloatValue(UNIT_FIELD_BASEATTACKTIME) / 1000.0f;
                    float const dps    = swing > 0.0f ? (dmgMin + dmgMax) * 0.5f / swing : 0.0f;
                    handler->PSendSysMessage("  slot {}: entry {}  hp {}/{}  hit {:.0f}-{:.0f}  swing {:.2f}s  (~{:.0f} dps)",
                        slot, c->GetEntry(), c->GetHealth(), c->GetMaxHealth(), dmgMin, dmgMax, swing, dps);
                }
                ++slot;
            }
        }
        return true;
    }
};

void AddSC_legion_commandscript()
{
    new legion_commandscript();
}
