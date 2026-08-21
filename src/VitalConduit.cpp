/*
 * mod-demonology-rework — Vital Conduit (vc), dead-node redesign A.1.
 *
 * Your Life Tap also heals your commanded demons for VitalConduit.HealPct of the health you
 * sacrifice, split among the injured ones (the vc->bt "circulatory" column: you feed the legion,
 * the legion feeds you). The life-for-power trade is preserved — the heal is paid in the warlock's
 * own health, every Tap. Bound to the Life Tap chain via spell_script_names (see data/sql base).
 *
 * Qualification (per addendum §A.5): this is a HEAL — it feeds nothing (no Soul Harvest, no
 * Doombrand, no Blood Tithe, no gb/fcd proc rolls). The amount is computed directly from the
 * health paid and cast with IGNORE_CASTER_MODIFIERS so spell power never inflates it.
 */
#include "CommandPool.h"
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"

#include "Creature.h"
#include "ObjectAccessor.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellScript.h"

#include <string>
#include <unordered_map>
#include <vector>

using namespace Demonology;

namespace
{
    // Last-Tap summary per owner, for `.legion dumpstats` (SpellScript instances are per-cast).
    std::unordered_map<ObjectGuid, std::string> g_lastVcTap;
}

std::string Demonology_GetLastVcTap(ObjectGuid owner)
{
    auto it = g_lastVcTap.find(owner);
    return it != g_lastVcTap.end() ? it->second : "(no Life Tap this session)";
}

class spell_demonology_life_tap_conduit : public SpellScript
{
    PrepareSpellScript(spell_demonology_life_tap_conduit);

    void HandleBeforeCast()
    {
        if (Player* p = GetCaster() ? GetCaster()->ToPlayer() : nullptr)
            _healthBefore = p->GetHealth();
    }

    void HandleAfterCast()
    {
        Player* owner = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        if (!owner)
            return;
        float const frac = Demonology::VitalConduitHeal(owner);
        if (frac <= 0.0f)
            return;

        int32 const paid = int32(_healthBefore) - int32(owner->GetHealth());   // health the Tap removed
        if (paid <= 0)
            return;

        // ALL living commanded demons: anchor pet + legionnaires + active greater demon. We do NOT
        // filter to injured demons — the warlock paid the health cost either way, so the heal always
        // fires and overheal on a full-health demon is simply wasted (allow-overheal, per feedback).
        // Wild Imps are excluded by construction (not the pet, not in the pool).
        std::vector<Creature*> recipients;
        if (Pet* pet = owner->GetPet())
            if (pet->IsAlive())
                recipients.push_back(pet);
        if (CommandPool* pool = sCommandPoolMgr->Find(owner->GetGUID()))
        {
            for (ObjectGuid g : pool->Legionnaires())
                if (Creature* c = ObjectAccessor::GetCreature(*owner, g))
                    if (c->IsAlive())
                        recipients.push_back(c);
            if (Creature* gd = ObjectAccessor::GetCreature(*owner, pool->GreaterDemonGuid()))
                if (gd->IsAlive())
                    recipients.push_back(gd);
        }

        if (recipients.empty())
        {
            g_lastVcTap[owner->GetGUID()] = "paid " + std::to_string(paid) + " health, no living demons";
            return;
        }

        int32 const each = int32(float(paid) * frac) / int32(recipients.size());
        if (each <= 0)
            return;
        for (Creature* c : recipients)
            owner->CastCustomSpell(SPELL_VITAL_CONDUIT_HEAL, SPELLVALUE_BASE_POINT0, each, c, true);

        g_lastVcTap[owner->GetGUID()] = "paid " + std::to_string(paid) + " health -> "
            + std::to_string(recipients.size()) + " demon(s), " + std::to_string(each) + " each";
    }

    void Register() override
    {
        BeforeCast += SpellCastFn(spell_demonology_life_tap_conduit::HandleBeforeCast);
        AfterCast  += SpellCastFn(spell_demonology_life_tap_conduit::HandleAfterCast);
    }

private:
    uint32 _healthBefore = 0;
};

void AddSC_demonology_vital_conduit()
{
    RegisterSpellScript(spell_demonology_life_tap_conduit);
}
