/*
 * mod-demonology-rework — shared IDs.
 *
 * Mirrors data/spellforge/ids.yaml (the authoring source of truth) and the
 * world-DB creature entries. Keep in sync by hand — the C++ needs literals.
 */
#ifndef MOD_DEMONOLOGY_REWORK_IDS_H
#define MOD_DEMONOLOGY_REWORK_IDS_H

#include "Define.h"   // AzerothCore's uint32 typedef

namespace Demonology
{
    // Player-facing spells (290000-290499)
    constexpr uint32 SPELL_DEMONIC_EMPOWERMENT = 290000;
    constexpr uint32 SPELL_SUMMON_WILD_IMPS    = 290001;
    constexpr uint32 SPELL_SUMMON_LEGIONNAIRE_IMP      = 290002;
    constexpr uint32 SPELL_SUMMON_LEGIONNAIRE_FELGUARD = 290003;
    constexpr uint32 SPELL_SUMMON_LEGIONNAIRE_SUCCUBUS = 290004;
    constexpr uint32 SPELL_SUMMON_LEGIONNAIRE_FELHOUND = 290005;
    constexpr uint32 SPELL_SUMMON_LEGIONNAIRE_VOIDWALKER = 290006;
    constexpr uint32 SPELL_SUMMON_INFERNAL     = 290007;
    constexpr uint32 SPELL_SUMMON_DOOMGUARD    = 290008;
    constexpr uint32 SPELL_SOUL_HARVEST_R1     = 290010;
    constexpr uint32 SPELL_SOUL_HARVEST_R2     = 290011;
    constexpr uint32 SPELL_SOUL_HARVEST_R3     = 290012;

    // Internal/hidden spells (290500-290899)
    constexpr uint32 SPELL_DEMONIC_EMPOWERMENT_BUFF = 290500;

    // Pet ability spells (290900-291199)
    constexpr uint32 SPELL_WILD_IMP_FIREBOLT = 290900;
    constexpr uint32 SPELL_DOOM_BOLT         = 290901;
    constexpr uint32 SPELL_DOOM_BLAST        = 40878;   // core AOE that Doom Bolt triggers (damage capped by us)

    // Talent marker spells (rank-1 spell of each node, pinned in content ids.yaml).
    // These are NOT added to the spell book (addToSpellBook=0), so test them with
    // Player::HasTalent(id, spec) — NOT HasSpell. See talent-effects-use-hastalent.
    constexpr uint32 SPELL_TALENT_EXPANDED_COMMAND    = 290922;  // ec  -> +1 legionnaire slot
    constexpr uint32 SPELL_TALENT_EXPANDED_COMMAND_II = 290932;  // ec2 -> +1 legionnaire slot
    constexpr uint32 SPELL_TALENT_LEGION_COMMANDER    = 290971;  // lc  -> +1 legionnaire slot
    constexpr uint32 SPELL_TALENT_SUMMON_FELGUARD     = 290948;  // sfg -> gates Felguard (pet + legionnaire)

    // Demon-scaling talent markers (rank-1 spell; a node's ranks are CONSECUTIVE ids, so
    // TalentRank(owner, rank1, maxRank) reads the trained rank). All read via HasTalent.
    constexpr uint32 SPELL_TALENT_FEL_CONDITIONING = 290904;  // fc [5,10,15]% demon health
    constexpr uint32 SPELL_TALENT_CURSED_VITALITY  = 290907;  // cv demons +[6,12]% stamina (~health); owner part TODO
    constexpr uint32 SPELL_TALENT_FEL_ARMORY       = 290912;  // fa [5,10,15]% demon damage while Fel Armor up
    constexpr uint32 SPELL_TALENT_SAVAGE_INSTINCTS = 290925;  // si [4,8,12]% demon attack speed
    constexpr uint32 SPELL_TALENT_VICIOUS_PACT     = 290933;  // vp [8,16,24]% SP->melee, [5,10,15]% SP->spell
    constexpr uint32 SPELL_TALENT_ETERNAL_SERVITUDE = 290965; // es: Infernal/Doomguard permanent + 60s CD

    // Phase 1 talent markers (verified rank-1 spell ids in Talent.dbc tab 303; ranks are
    // consecutive ids so TalentRank reads the trained rank). All read via HasTalent.
    constexpr uint32 SPELL_TALENT_PACTBOUND_FURY        = 290917;  // pf  [2,4,6]% demon crit (damage hook)
    constexpr uint32 SPELL_TALENT_DEMONIC_REBIRTH       = 290940;  // dr  50/100% instant resummon, 60s ICD
    constexpr uint32 SPELL_TALENT_OVERLORDS_PRESENCE    = 290956;  // op  per commanded demon: owner +HP/+haste
    constexpr uint32 SPELL_TALENT_BOUND_BY_BLOOD        = 290974;  // bbb on demon death: survivors +dmg/haste, refund 1 shard
    constexpr uint32 SPELL_TALENT_GRAND_WARLOCKS_DESIGN = 290976;  // gwd capstone (Legion Aura groundwork here; full rider Phase 5)

    // Core base pet-summon spells (warlocks learn these normally). Knowing one + the
    // Expanded Command talent grants that type's "Summon <type> Legionnaire" (hybrid
    // learn). Felguard is the exception: its PET is itself gated behind the sfg talent.
    constexpr uint32 SPELL_SUMMON_IMP_PET        = 688;
    constexpr uint32 SPELL_SUMMON_FELHUNTER_PET  = 691;
    constexpr uint32 SPELL_SUMMON_VOIDWALKER_PET = 697;
    constexpr uint32 SPELL_SUMMON_SUCCUBUS_PET   = 712;
    constexpr uint32 SPELL_SUMMON_FELGUARD_PET   = 30146;

    // Vanilla greater-demon summons (temporary, all specs). Eternal Servitude suppresses
    // these so an ES warlock only uses our permanent Summon Infernal/Doomguard.
    constexpr uint32 SPELL_VANILLA_INFERNO        = 1122;   // Inferno (temp Infernal)
    constexpr uint32 SPELL_VANILLA_RITUAL_OF_DOOM = 18540;  // Ritual of Doom (temp Doomguard)

    // Creatures (600000-600099, world DB)
    constexpr uint32 NPC_WILD_IMP = 600000;

    // Base pet creature entries reused as legionnaire guardians (runtime AI attached).
    constexpr uint32 NPC_BASE_IMP        = 416;
    constexpr uint32 NPC_BASE_FELHUNTER  = 417;
    constexpr uint32 NPC_BASE_VOIDWALKER = 1860;
    constexpr uint32 NPC_BASE_SUCCUBUS   = 1863;
    constexpr uint32 NPC_BASE_FELGUARD   = 17252;

    // Greater demons (temporary guardians, base entries). NPC_BASE_* prefix avoids
    // colliding with the core's own NPC_INFERNAL / NPC_DOOMGUARD pet enums.
    constexpr uint32 NPC_BASE_INFERNAL  = 89;
    constexpr uint32 NPC_BASE_DOOMGUARD = 11859;

    // Items
    constexpr uint32 ITEM_SOUL_SHARD = 6265;
}

#endif // MOD_DEMONOLOGY_REWORK_IDS_H
