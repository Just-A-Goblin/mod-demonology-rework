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

// Last Vital Conduit Life Tap summary (defined in VitalConduit.cpp).
std::string Demonology_GetLastVcTap(ObjectGuid owner);

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
            { "command",   HandleCommandCommand,   SEC_GAMEMASTER, Console::No },
            { "brand",     HandleBrandCommand,     SEC_GAMEMASTER, Console::No },
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

    static bool HandleCommandCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;
        Demonology::CommandDemonPress(player);
        handler->PSendSysMessage("[legion] Command Demon press -> {}", Demonology::GetLastCommandPress(player->GetGUID()));
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
        handler->PSendSysMessage("[legion] scaling talents: Fel Conditioning={} Vicious Pact={} Savage Instincts={} Fel Armory={} (Fel Armor up={}) | dmgMult melee={:.2f} spell={:.2f}",
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_FEL_CONDITIONING, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_VICIOUS_PACT, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_SAVAGE_INSTINCTS, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_FEL_ARMORY, 3)),
            uint32(Demonology::OwnerHasFelArmor(player)),
            meleeMult, spellMult);
        // dmgMult melee above is the GUARDIAN value; the anchor pet instead gets Vicious Pact's +
        // (armor-up) Fel Armory melee halves as a real % damage modifier (shows in the pet tab DPS).
        handler->PSendSysMessage("[legion] anchor-pet melee bonus (Vicious Pact + Fel Armory) = +{:.0f}%",
            Demonology::AnchorPetMeleeDamagePct(player) * 100.0f);
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

        // --- Phase 1 talents + live state (demon-death hooks, owner auras, crit rolls) ---
        handler->PSendSysMessage("[legion] Phase 1 talents: Pactbound Fury={} Demonic Rebirth={} Overlord's Presence={} Bound by Blood={} Cursed Vitality={}",
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_PACTBOUND_FURY, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_DEMONIC_REBIRTH, 2)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_OVERLORDS_PRESENCE, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_BOUND_BY_BLOOD, 2)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_CURSED_VITALITY, 2)));

        if (Demonology::CommandPool* p1 = sCommandPoolMgr->Find(player->GetGUID()))
        {
            uint8 const cmd = p1->CommandedDemonCount(player);
            handler->PSendSysMessage("[legion] Overlord's Presence: {} commanded demon(s) -> owner +{:.1f}% max HP, +{:.1f}% haste  (Cursed Vitality +{:.0f}% stamina)",
                uint32(cmd),
                Demonology::OverlordsPresenceHealthPerDemon(player) * float(cmd) * 100.0f,
                Demonology::OverlordsPresenceHastePerDemon(player) * float(cmd) * 100.0f,
                Demonology::CursedVitalityOwnerStamina(player) * 100.0f);

            uint32 const drCd = p1->RebirthReadyInMs();
            handler->PSendSysMessage("[legion] Demonic Rebirth: chance {:.0f}%, {} (ICD {}s)",
                Demonology::DemonicRebirthChance(player) * 100.0f,
                drCd ? "on cooldown" : "ready", drCd / 1000);

            float const bloodDmg = p1->LegionBuffDamage();
            uint32 const bloodCd = p1->BloodBuffReadyInMs();
            if (bloodDmg > 0.0f)
                handler->PSendSysMessage("[legion] Bound by Blood: WINDOW ACTIVE — survivors +{:.0f}% damage, +{:.0f}% haste",
                    bloodDmg * 100.0f, p1->LegionBuffHaste() * 100.0f);
            else
                handler->PSendSysMessage("[legion] Bound by Blood: buff {} (on death: +{:.0f}% damage/+{:.0f}% haste {}s, CD {}s; refund {})",
                    bloodCd ? "on cooldown" : "ready",
                    Demonology::BoundByBloodDamage(player) * 100.0f, Demonology::BoundByBloodHaste(player) * 100.0f,
                    Demonology::gConfig.BoundByBloodDurationMs / 1000, Demonology::gConfig.BoundByBloodIcdMs / 1000,
                    Demonology::gConfig.BoundByBloodRefundShard ? "1 shard" : "off");
        }

        handler->PSendSysMessage("[legion] Pactbound Fury: +{:.0f}% crit — REAL crits on both melee (5%+) and spell (5% base +{:.0f}%), all visible",
            Demonology::PactboundFuryCritChance(player) * 100.0f,
            Demonology::PactboundFuryCritChance(player) * 100.0f);

        // Phase 2 shard economy (Path B): what actives cost right now.
        handler->PSendSysMessage("[legion] shard costs: Summon Wild Imps {} (Improved Legion: {}), legionnaire (re)summon {}  [you hold {}]",
            Demonology::WildImpShardCost(player),
            Demonology::TalentRank(player, Demonology::SPELL_TALENT_IMPROVED_LEGION, 2) ? "trained" : "untrained",
            Demonology::gConfig.SummonLegionnaireShardCost,
            player->GetItemCount(SOUL_SHARD_ITEM));
        handler->PSendSysMessage("[legion] Cruel Master: rank {} — on a sim-crit ({:.0f}% chance): x{:.1f} proc chance, rank2 x{:.2f} ICD",
            uint32(Demonology::CruelMasterRank(player)),
            Demonology::DemonSimCritChance(player) * 100.0f,
            Demonology::gConfig.CruelMasterProcChanceMult,
            Demonology::gConfig.CruelMasterIcdMultOnCrit);

        // Phase 3 Command Demon: Dark Command rank + last-press breakdown.
        handler->PSendSysMessage("[legion] Command Demon: Dark Command rank {} (CD {}s) | last press: {}",
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_DARK_COMMAND, 3)),
            (Demonology::gConfig.CommandDemonCooldownMs
                - Demonology::TalentRank(player, Demonology::SPELL_TALENT_DARK_COMMAND, 3) * Demonology::gConfig.DarkCommandCdReductionMsPerRank) / 1000,
            Demonology::GetLastCommandPress(player->GetGUID()));

        // Phase 4 Empowerment spine — the buff a Demonic Empowerment cast would apply now.
        handler->PSendSysMessage("[legion] Empowerment spine: sl={} uv={} cotp={} re={} se={}",
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_SHADOWFLAME_LEGION, 2)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_UNHOLY_VIGOR, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_CRUELTY_OF_THE_PIT, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_RUINOUS_EMPOWERMENT, 3)),
            uint32(Demonology::TalentRank(player, Demonology::SPELL_TALENT_SUPREME_EMPOWERMENT, 2)));
        handler->PSendSysMessage("[legion]   buff now: +{:.0f}% dmg, +{:.0f}% haste, shield {:.0f}% max HP, {}s{}, leech {:.0f}%, refresh {:.0f}%",
            (Demonology::gConfig.DemonicEmpowermentDamage + Demonology::CrueltyOfThePitDamage(player)) * 100.0f,
            Demonology::gConfig.DemonicEmpowermentHaste * 100.0f,
            Demonology::ShadowflameLegionAbsorb(player) * 100.0f,
            (Demonology::gConfig.DemonicEmpowermentDurationMs + Demonology::UnholyVigorDurationMs(player) + Demonology::SupremeEmpowermentDurationMs(player)) / 1000,
            Demonology::SupremeEmpowermentTrained(player) ? " (temps too)" : " (permanent demons only)",
            Demonology::RuinousEmpowermentLeech(player) * 100.0f,
            Demonology::RuinousEmpowermentNoExpire(player) * 100.0f);

        // Phase 5 Doombrand — capstone known-state + the live accumulator (there's no client
        // charge gauge, so the sigil's stored damage is surfaced here, per §6.5).
        bool const gwd = player->HasTalent(Demonology::SPELL_TALENT_GRAND_WARLOCKS_DESIGN, player->GetActiveSpec());
        float const brandCap = Demonology::gConfig.DoombrandCapSPCoef * float(sp);
        handler->PSendSysMessage("[legion] Doombrand: {} — store {:.0f}% of demon damage, cap {:.0f} (={:.1f} x SP), cost {} shard",
            gwd ? "TRAINED (Grand Warlock's Design)" : "not trained",
            Demonology::gConfig.DoombrandStorePct * 100.0f, brandCap,
            Demonology::gConfig.DoombrandCapSPCoef, Demonology::gConfig.DoombrandShardCost);
        if (Demonology::CommandPool* pool = sCommandPoolMgr->Find(player->GetGUID()))
        {
            if (!pool->BrandTarget().IsEmpty())
            {
                Unit* bt = ObjectAccessor::GetUnit(*player, pool->BrandTarget());
                handler->PSendSysMessage("[legion]   ACTIVE brand on {} — stored {:.0f} / {:.0f} ({:.0f}%){}",
                    bt ? bt->GetName() : "<gone>", pool->BrandStored(), brandCap,
                    brandCap > 0.0f ? (pool->BrandStored() / brandCap * 100.0f) : 0.0f,
                    pool->BrandStored() >= brandCap && brandCap > 0.0f ? " [AT CAP]" : "");
            }
            else
                handler->PSendSysMessage("[legion]   no active brand");
        }

        // Phase 6 talents (batch A: Blood Tithe + Beacon of Ruin).
        {
            uint8 const commanded = sCommandPoolMgr->Find(player->GetGUID())
                ? sCommandPoolMgr->Find(player->GetGUID())->CommandedDemonCount(player) : 0;
            handler->PSendSysMessage("[legion] Blood Tithe: heal {:.0f}% of demon damage ({} demons -> {})",
                Demonology::BloodTitheHeal(player, commanded) * 100.0f, commanded,
                commanded >= Demonology::gConfig.BloodTitheDoubleAtDemons ? "DOUBLED" : "base");
            uint8 const bor = Demonology::TalentRank(player, Demonology::SPELL_TALENT_BEACON_OF_RUIN, 2);
            handler->PSendSysMessage("[legion] Beacon of Ruin: rank {} -> greater demons +{:.0f}% damage, summon CD -{:.0f}%",
                uint32(bor), (bor ? Demonology::gConfig.BeaconOfRuinDamagePct[bor - 1] : 0.0f) * 100.0f,
                Demonology::BeaconOfRuinCdReduction(player) * 100.0f);
            handler->PSendSysMessage("[legion] Improved Wild Imps: +{}s duration, {:.0f}% Firebolt 2nd-target  |  Wrath of the Legion: {:.0f}% spawn (max {}/cast)",
                Demonology::ImprovedWildImpsDurationMs(player) / 1000,
                Demonology::ImprovedWildImpsSecondTargetChance(player) * 100.0f,
                Demonology::WrathOfTheLegionSpawnChance(player) * 100.0f,
                uint32(Demonology::gConfig.WrathOfTheLegionMaxChainsPerCast));
            handler->PSendSysMessage("[legion] Warded Legion: {:.0f}% spell resist{}  |  Grim Bargain: {:.0f}% chance -> +{:.0f}% dmg  |  Fel Conduit: {:.0f}% proc  |  Fel Blood: Lash +{:.0f}%",
                Demonology::WardedLegionResist(player) * 100.0f,
                Demonology::WardedLegionCcImmune(player) ? " + Fear/Charm/Poly immune" : "",
                Demonology::GrimBargainProcChance(player) * 100.0f, Demonology::GrimBargainDamage(player) * 100.0f,
                Demonology::FelConduitProc(player) * 100.0f,
                Demonology::FelBloodLash(player) * 100.0f);
            handler->PSendSysMessage("[legion] Riftwalker: {} (Demonic Circle: Teleport warps demons + {}% speed {}s)",
                Demonology::RiftwalkerTrained(player) ? "TRAINED" : "not trained",
                Demonology::gConfig.RiftwalkerMoveSpeedPct, Demonology::gConfig.RiftwalkerDurationMs / 1000);
            handler->PSendSysMessage("[legion] Vital Conduit: Life Tap heals demons for {:.0f}% of health paid  |  last Tap: {}",
                Demonology::VitalConduitHeal(player) * 100.0f, Demonology_GetLastVcTap(player->GetGUID()));
        }
        return true;
    }

    // `.legion brand` — cast Doombrand on the current selection (test tooling; the real cast
    // is the 290014 button granted by Grand Warlock's Design).
    static bool HandleBrandCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;
        Unit* target = handler->getSelectedUnit();
        if (!target || target == player || player->IsFriendlyTo(target))
        {
            handler->PSendSysMessage("[legion] Select a hostile target to brand.");
            return true;
        }
        player->CastSpell(target, Demonology::SPELL_DOOMBRAND, true);
        handler->PSendSysMessage("[legion] Doombrand cast on {}.", target->GetName());
        return true;
    }
};

void AddSC_legion_commandscript()
{
    new legion_commandscript();
}
