/*
 * mod-demonology-rework — talent helpers.
 *
 * Small inline readers used to wire self-contained talent effects into the demon
 * systems. Talent RANKS live in the talent map (not the spell book — our markers
 * have addToSpellBook=0), so they must be read via Player::HasTalent, and a node's
 * ranks are allocated as CONSECUTIVE spell ids from its rank-1 marker.
 */
#ifndef MOD_DEMONOLOGY_REWORK_TALENTS_H
#define MOD_DEMONOLOGY_REWORK_TALENTS_H

#include "Define.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "Creature.h"
#include "Player.h"
#include "SharedDefines.h"

namespace Demonology
{
    // The warlock owner of an owned demon (anchor pet, legionnaire, or Wild Imp), or
    // nullptr if the unit isn't a creature owned by a warlock. Used to scale demon
    // damage from any of them uniformly.
    inline Player* WarlockOwnerOfDemon(Unit* demon)
    {
        if (!demon || demon->GetTypeId() != TYPEID_UNIT)
            return nullptr;
        Unit* owner = demon->GetOwner();
        Player* p = owner ? owner->ToPlayer() : nullptr;
        return (p && p->getClass() == CLASS_WARLOCK) ? p : nullptr;
    }

    // Trained rank of a talent: 0 = untrained, else 1..maxRank. Ranks are consecutive
    // marker spells starting at rank1Spell (see the header note).
    inline uint8 TalentRank(Player* owner, uint32 rank1Spell, uint8 maxRank)
    {
        if (!owner)
            return 0;
        uint8 const spec = owner->GetActiveSpec();
        for (uint8 r = maxRank; r >= 1; --r)
            if (owner->HasTalent(rank1Spell + (r - 1), spec))
                return r;
        return 0;
    }

    // Is any rank of Fel Armor active on the owner? (Fel Armory keys off it.)
    inline bool OwnerHasFelArmor(Player* owner)
    {
        static constexpr uint32 FEL_ARMOR[] = { 28176, 28189, 44520, 44977, 47892, 47893 };
        for (uint32 id : FEL_ARMOR)
            if (owner->HasAura(id))
                return true;
        return false;
    }

    // Combined demon DAMAGE multiplier from the scaling talents, additive on top of 1.0:
    //   Vicious Pact  — SP-as-attack-power on melee ([8,16,24]%) or SP-as-spell-power on
    //                   spells ([5,10,15]%); pass meleeSide to pick which.
    //   Fel Armory    — [5,10,15]% while the owner's Fel Armor is up.
    inline float DemonDamageMult(Player* owner, bool meleeSide)
    {
        if (!owner)
            return 1.0f;
        float mult = 1.0f;
        if (uint8 vp = TalentRank(owner, SPELL_TALENT_VICIOUS_PACT, 3))
            mult += meleeSide ? gConfig.VpMeleePct[vp - 1] : gConfig.VpSpellPct[vp - 1];
        if (uint8 fa = TalentRank(owner, SPELL_TALENT_FEL_ARMORY, 3))
            if (OwnerHasFelArmor(owner))
                mult += gConfig.FaDamagePct[fa - 1];
        return mult;
    }

    // Demon HEALTH multiplier (>=1.0): Fel Conditioning +[5,10,15]% and Cursed Vitality's
    // demon-stamina part +[6,12]% (the owner-stamina part is a separate player buff, TODO).
    inline float DemonHealthMult(Player* owner)
    {
        if (!owner)
            return 1.0f;
        float mult = 1.0f;
        if (uint8 fc = TalentRank(owner, SPELL_TALENT_FEL_CONDITIONING, 3))
            mult += gConfig.FcHealthPct[fc - 1];
        if (uint8 cv = TalentRank(owner, SPELL_TALENT_CURSED_VITALITY, 2))
            mult += gConfig.CvDemonHealthPct[cv - 1];
        return mult;
    }

    // Demon melee-haste PERCENT (e.g. 12.0 for +12% attack speed) from Savage Instincts.
    inline float DemonHastePct(Player* owner)
    {
        if (uint8 si = TalentRank(owner, SPELL_TALENT_SAVAGE_INSTINCTS, 3))
            return gConfig.SiHastePct[si - 1];
        return 0.0f;
    }
}

#endif // MOD_DEMONOLOGY_REWORK_TALENTS_H
