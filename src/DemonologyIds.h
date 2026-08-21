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
    constexpr uint32 SPELL_COMMAND_DEMON       = 290013;  // Phase 3 baseline Command Demon button
    constexpr uint32 SPELL_DOOMBRAND           = 290014;  // Phase 5 capstone (granted by Grand Warlock's Design)

    // Internal/hidden spells (290500-290899)
    constexpr uint32 SPELL_DEMONIC_EMPOWERMENT_BUFF   = 290500;
    constexpr uint32 SPELL_ENTHRALLING_PRESENCE       = 290501;  // Succubus signature enemy debuff
    constexpr uint32 SPELL_EMPOWERED_LASH_OF_PAIN     = 290502;  // Succubus echo nuke
    constexpr uint32 SPELL_INFERNAL_COMMAND_PULSE     = 290503;  // Infernal greater-demon response (fire AoE)
    constexpr uint32 SPELL_VOIDWALKER_COMMAND_SHIELD  = 290506;  // Voidwalker signature owner absorb
    constexpr uint32 SPELL_VOIDWALKER_CONSUME_SHADOWS = 290507;  // Voidwalker echo self-shield
    constexpr uint32 SPELL_DOOMBRAND_DEBUFF           = 290504;  // Doombrand marker/accumulator debuff
    constexpr uint32 SPELL_DOOMBRAND_DETONATION       = 290505;  // Doombrand stored-damage shadowflame hit
    constexpr uint32 SPELL_WARDED_LEGION_WARD         = 290508;  // wl demon spell-resist ward (amount set live)
    constexpr uint32 SPELL_GRIM_BARGAIN_OWNER         = 290509;  // gb owner damage buff (8s)
    constexpr uint32 SPELL_GRIM_BARGAIN_DEMON         = 290510;  // gb demon damage buff (8s)
    constexpr uint32 SPELL_FEL_CONDUIT_CHARGE         = 290511;  // fcd instant/free Shadow Bolt charge (3 stacks)
    constexpr uint32 SPELL_RIFTWALKER_HASTE           = 290512;  // rw demon move-speed burst (6s)
    constexpr uint32 SPELL_FELGUARD_CLEAVE            = 290513;  // Felguard legionnaire signature (cleave)
    constexpr uint32 SPELL_FELHUNTER_SHADOWBITE       = 290514;  // Felhunter legionnaire signature (shadow nuke)
    constexpr uint32 SPELL_SUCCUBUS_LASH              = 290515;  // Succubus legionnaire signature (shadow nuke)
    constexpr uint32 SPELL_VITAL_CONDUIT_HEAL         = 290516;  // vc: Life Tap heals the legion (dead-node redesign)
    constexpr uint32 SPELL_FERVENT_STANDARD_ICON      = 290517;  // fs: owner-only cosmetic icon while inside the circle (no math)

    // Baseline warlock spells the module reads (not ours; core IDs).
    constexpr uint32 SPELL_DEMONIC_CIRCLE_SUMMON      = 48018;   // fs anchor: the circle GameObject; rw teleport dest

    // Vanilla pet/demon abilities the Command dispatcher casts (rank-1 chain anchors — the
    // TOP rank is resolved from spell_dbc at startup, never hardcoded; see CommandDemon.cpp).
    constexpr uint32 SPELL_VANILLA_INTERCEPT_R1  = 30151;  // Felguard signature (charge + stun)
    constexpr uint32 SPELL_VANILLA_CLEAVE_R1     = 30213;  // Felguard signature + echo
    constexpr uint32 SPELL_VANILLA_SPELL_LOCK_R1 = 19244;  // Felhound signature (interrupt/silence)
    constexpr uint32 SPELL_VANILLA_SUFFERING_R1  = 17735;  // Voidwalker signature (AoE taunt)
    constexpr uint32 SPELL_VANILLA_IMP_FIREBOLT_R1 = 3110; // Imp signature (volley) + echo
    constexpr uint32 SPELL_VANILLA_GROWL_R1        = 2649;  // Voidwalker echo taunt (attack-me)
    constexpr uint32 SPELL_VANILLA_LASH_OF_PAIN_R1 = 7814;  // Succubus Lash of Pain (Fel Blood boosts it)
    constexpr uint32 SPELL_VANILLA_CORRUPTION_R1   = 172;   // Warlock Corruption (Fel Blood refreshes it on a demon kill)
    constexpr uint32 SPELL_VANILLA_LIFE_TAP_R1     = 1454;  // Warlock Life Tap (Vital Conduit heals demons off the sacrificed health)

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
    constexpr uint32 SPELL_TALENT_IMPROVED_LEGION  = 290902;  // il Wild Imps cost -1 shard (single-effect; cast-time half removed A.4)
    constexpr uint32 SPELL_TALENT_CRUEL_MASTER     = 290915;  // cm demon crits accelerate Soul Harvest (2 ranks)
    constexpr uint32 SPELL_TALENT_FEL_CONDITIONING = 290904;  // fc [5,10,15]% demon health
    constexpr uint32 SPELL_TALENT_CURSED_VITALITY  = 290907;  // cv demons +[6,12]% stamina (~health) + owner +[3,6]% stamina (OwnerMods)
    constexpr uint32 SPELL_TALENT_FEL_ARMORY       = 290912;  // fa [5,10,15]% demon damage while Fel Armor up
    constexpr uint32 SPELL_TALENT_SAVAGE_INSTINCTS = 290925;  // si [4,8,12]% demon attack speed
    constexpr uint32 SPELL_TALENT_VICIOUS_PACT     = 290933;  // vp [8,16,24]% SP->melee, [5,10,15]% SP->spell
    constexpr uint32 SPELL_TALENT_ETERNAL_SERVITUDE = 290965; // es: Infernal/Doomguard permanent + 60s CD

    // Phase 1 talent markers (verified rank-1 spell ids in Talent.dbc tab 303; ranks are
    // consecutive ids so TalentRank reads the trained rank). All read via HasTalent.
    constexpr uint32 SPELL_TALENT_PACTBOUND_FURY        = 290917;  // pf  [2,4,6]% demon crit (damage hook)
    constexpr uint32 SPELL_TALENT_DEMONIC_REBIRTH       = 290940;  // dr  50/100% instant resummon, 60s ICD
    constexpr uint32 SPELL_TALENT_DARK_COMMAND          = 290942;  // dc  Command Demon CD -5/10/15s + responder haste
    constexpr uint32 SPELL_TALENT_OVERLORDS_PRESENCE    = 290956;  // op  per commanded demon: owner +HP/+haste
    constexpr uint32 SPELL_TALENT_BOUND_BY_BLOOD        = 290974;  // bbb on demon death: survivors +dmg/haste, refund 1 shard
    constexpr uint32 SPELL_TALENT_GRAND_WARLOCKS_DESIGN = 290976;  // gwd capstone — grants Doombrand (290014)

    // Phase 6 talent markers (rank-1 spell ids; ranks are consecutive; read via HasTalent).
    constexpr uint32 SPELL_TALENT_RAPID_CONJURATION   = 290909;  // rc   summon cast -1.5/3/4.5s; R3 move-cast
    constexpr uint32 SPELL_TALENT_FEL_BLOOD           = 290920;  // fb   Lash of Pain +dmg; killing blow refreshes Corruption
    constexpr uint32 SPELL_TALENT_VITAL_CONDUIT       = 290923;  // vc   Health Funnel +efficiency, no self-damage
    constexpr uint32 SPELL_TALENT_IMPROVED_WILD_IMPS  = 290930;  // iwi  Wild Imps +duration; Firebolt 2nd target
    constexpr uint32 SPELL_TALENT_BLOOD_TITHE         = 290936;  // bt   demon damage heals owner (doubled at 3+ demons)
    constexpr uint32 SPELL_TALENT_WARDED_LEGION       = 290938;  // wl   demon spell resist chance; R2 Fear/Charm/Poly immune
    constexpr uint32 SPELL_TALENT_WRATH_OF_THE_LEGION = 290949;  // wotl Firebolt chance to spawn an extra Wild Imp
    constexpr uint32 SPELL_TALENT_GRIM_BARGAIN        = 290952;  // gb   demon<->owner damage proc synergy
    constexpr uint32 SPELL_TALENT_FEL_CONDUIT         = 290959;  // fcd  demon attacks proc instant/free Shadow Bolt
    constexpr uint32 SPELL_TALENT_BEACON_OF_RUIN      = 290969;  // bor  Infernal/Doomguard +dmg / -summon CD
    constexpr uint32 SPELL_TALENT_RIFTWALKER          = 290961;  // rw   Demonic Circle: Teleport warps demons + speed
    constexpr uint32 SPELL_TALENT_FERVENT_STANDARD    = 290954;  // fs   Demonic Circle = legion banner: +dmg / demons -dmg taken in radius

    // Phase 4 — Demonic Empowerment spine (all enhance the 290000/290500 empowerment).
    constexpr uint32 SPELL_TALENT_SHADOWFLAME_LEGION    = 290928;  // sl   Empowerment also shields demons ([15,30]% max HP)
    constexpr uint32 SPELL_TALENT_UNHOLY_VIGOR          = 290945;  // uv   Empowerment +[1,2,3]s
    constexpr uint32 SPELL_TALENT_CRUELTY_OF_THE_PIT    = 290962;  // cotp empowered demons +[5,10,15]% damage
    constexpr uint32 SPELL_TALENT_RUINOUS_EMPOWERMENT   = 290966;  // re   empowered demons leech + [7,14,20]% no-expire
    constexpr uint32 SPELL_TALENT_SUPREME_EMPOWERMENT   = 290972;  // se   Empowerment affects temp demons + [3,6]s longer

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
