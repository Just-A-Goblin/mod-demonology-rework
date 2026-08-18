/*
 * mod-demonology-rework — Demonic Empowerment (PLAN §6).
 *
 * DBC spell 290000 is a dummy shell; this SpellScript selects the caster's
 * demons and applies the internal buff (290500) to each: the anchor pet, every
 * Command Pool legionnaire, and temporary demons (Wild Imps). Supreme Empowerment
 * (T10) will later gate the temporary-demon part by flipping one predicate.
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"

#include "Creature.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

#include <list>

using namespace Demonology;

class spell_demonology_demonic_empowerment : public SpellScript
{
    PrepareSpellScript(spell_demonology_demonic_empowerment);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!caster)
            return;

        // AddAura applies the aura DIRECTLY to the target, bypassing the buff's
        // implicit target (its effects use unit_caster, so CastSpell would land on
        // the player instead of the demon).
        auto empower = [caster](Unit* demon)
        {
            if (demon && demon->IsAlive())
                caster->AddAura(SPELL_DEMONIC_EMPOWERMENT_BUFF, demon);
        };

        // 1) The anchor pet.
        empower(caster->GetPet());

        // 2) Every Command Pool legionnaire + the active greater demon (Infernal/Doomguard).
        if (Demonology::CommandPool* pool = sCommandPoolMgr->Find(caster->GetGUID()))
        {
            for (ObjectGuid guid : pool->Legionnaires())
                empower(ObjectAccessor::GetCreature(*caster, guid));
            empower(ObjectAccessor::GetCreature(*caster, pool->GreaterDemonGuid()));
        }

        // 3) Temporary owned demons in range (Wild Imps) — gated by Supreme Empowerment later.
        std::list<Creature*> temps;
        caster->GetCreatureListWithEntryInGrid(temps, NPC_WILD_IMP, 100.0f);
        for (Creature* imp : temps)
            if (imp->GetOwnerGUID() == caster->GetGUID())
                empower(imp);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_demonology_demonic_empowerment::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_demonology_empowerment_spells()
{
    RegisterSpellScript(spell_demonology_demonic_empowerment);
}
