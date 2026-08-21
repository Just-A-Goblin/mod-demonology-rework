/*
 * mod-demonology-rework — Fel Corruption (rc redesign, Addendum A.2).
 *
 * Your Corruption is infested with fel energy: each of its periodic ticks counts as demon damage
 * for Soul Harvest and Doombrand at 33/66/100% effectiveness (per rank). The DoT becomes one more
 * demon in your service — it completes the T1–T2 economy column and makes Corruption upkeep matter
 * to your brand windows.
 *
 * Qualification (Addendum A.5 — the important part): the tick feeds EXACTLY Soul Harvest + the
 * Doombrand accumulator, via LegionEconomy::QualifyPlayerPeriodic. It does NOT trigger bt/gb/fcd/pf,
 * receive fa/vp/cotp demon multipliers, or count as a demon attack for anything else. Corruption
 * remains player damage in every respect except those two named qualifications. Bound to the
 * Corruption chain via spell_script_names (data/sql base).
 */
#include "DemonologyConfig.h"
#include "DemonologyIds.h"
#include "DemonologyTalents.h"
#include "LegionEconomy.h"

#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"

using namespace Demonology;

class spell_demonology_fel_corruption : public AuraScript
{
    PrepareAuraScript(spell_demonology_fel_corruption);

    void HandlePeriodic(AuraEffect const* aurEff)
    {
        Player* owner = GetCaster() ? GetCaster()->ToPlayer() : nullptr;
        Unit* target = GetUnitOwner();          // the DoT sits on the target
        if (!owner || !target)
            return;
        float const eff = Demonology::FelCorruptionEffectiveness(owner);
        if (eff <= 0.0f)
            return;
        // Feed Soul Harvest + Doombrand off this tick's damage, at rank effectiveness. Nothing else.
        Demonology::LegionEconomy::QualifyPlayerPeriodic(owner, target, float(aurEff->GetAmount()), eff);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_demonology_fel_corruption::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

void AddSC_demonology_fel_corruption()
{
    RegisterSpellScript(spell_demonology_fel_corruption);
}
