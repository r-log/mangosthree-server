/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// Tests of the spell handler registry.
///
/// The registry is tested without a map: its sites here are test sites with their own keys and contexts, and the game's
/// sites (HandleAuraDummy's, HandleAuraTransform's, HandleModThreat's, EffectTransmitted's, EffectEnergize's,
/// EffectActivateObject's, EffectResurrect's, the two of SetTargetMap, CheckCast's, CheckTarget's, EffectWeaponDmg's,
/// EffectSchoolDMG's, EffectTriggerSpell's, the two of EffectTeleportUnits, the five family switches of EffectDummy and
/// the five of SpellAuraPeriodic.cpp) are checked for their keys, their defaults and their contexts, and run where a
/// body needs no live Unit (the quest-tame labels, the removal labels on a mode that keeps them off the Unit, the
/// transmitted-object, energize, resurrect, targeting, cast-check and target-check defaults, the periodic-trigger
/// labels on such a mode, the health labels on an apply that is not real, the health default, the energize injector
/// labels on a bare Creature, the 54732 defibrillate roll, which has no failure spell, the area-target labels on bare
/// Creatures, the cast-check labels on a creature caster, the target-check labels on creature targets, the
/// weapon-damage and divided school-damage labels on a target list built by hand, the other school-damage labels on
/// creatures with their update fields, the Vanish, Cloak of Shadows, Shadowfiend and Mirror Image labels on creatures
/// with no aura and no pet, the recall label on creature targets, and the dummy-effect labels that end the effect with
/// no unit target, for a creature caster or for a caster with no pet);
/// a body that casts, sets a display, reads a level or an aura or acts on a game object needs a live Unit or game
/// object, which the harness record covers where a scenario reaches it (931: 41101 and 53790, applied and removed; the
/// coverage scenario two-feigns-one-lift: the feign-death body, through 29266 and 31261; no scenario reaches a druid,
/// quest-tame, transform, threat, transmitted-object, energize, activate-object, resurrect, targeting, cast-check,
/// target-check, weapon-damage, school-damage, trigger-spell, teleport, dummy-effect or periodic-aura label, nor
/// another removal one).
/// Each dispatch mutant the note names has a test here that kills it:
///   lost key                 SpellHandlerRegistry_FindReturnsTheRegisteredFunction,
///                            AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheTableRegistersEveryRowOnce,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault,
///                            AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheEnergizeSiteHoldsItsFiveLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheRogueSiteHoldsItsOneLabelAndNoDefault,
///                            AuraPeriodicHandlers_TheIncreaseHealthSiteHoldsItsFourteenLabelsAndTheDefault,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault
///   wrong site               SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            the five AuraPeriodicHandlers_The...SiteHolds... tests (no id at another site),
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault
///   default first            SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss,
///                            AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs
///   a lost default           AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault
///   a default added          SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault
///   Continue taken as Return SpellHandlerRegistry_ContinueAndReturnAreDistinct,
///                            AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts,
///                            AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault,
///                            AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault,
///                            AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs,
///                            SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault,
///                            SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault
///   Return taken as Continue SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault,
///                            SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault
///   a lost fall-through      AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs
///   a lost loop continue     SpellHandlerRegistry_LoopContinueIsAFourthOutcome,
///                            SpellHandlerRegistry_ALoopContinueSkipsTheRestOfTheLoopBody
///   a lost live-out          SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite,
///                            AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheApplyRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId,
///                            AuraShapeshiftHandlers_TheTransformContextAliasesTheTargetLocal,
///                            AuraControlHandlers_TheThreatContextAliasesTheThreeLocals,
///                            SpellEffectTailHandlers_TheTransmittedContextAliasesTheCasterAndTheEntry,
///                            the five AuraPeriodicHandlers_The...ContextAliases... tests,
///                            SpellEffectHealPowerHandlers_TheEnergizeContextAliasesTheSpellAndTheLevelLocals,
///                            SpellEffectObjectCombatHandlers_TheActivateObjectContextAliasesTheSpell,
///                            SpellEffectObjectCombatHandlers_TheResurrectContextAliasesTheSpell,
///                            SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault,
///                            SpellTargetingHandlers_TheAllEnemyInAreaContextAliasesTheSpellAndTheLocals,
///                            SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault,
///                            SpellTargetingHandlers_TheEffectDummyContextAliasesTheSpell,
///                            SpellChecksHandlers_TheCastAuraDummyContextAliasesTheCaster,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetContextAliasesTheCasterAndTheTarget,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault,
///                            SpellEffectSkillEnchantPetHandlers_TheWeaponDamageContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTriggerSpellContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTeleportRecallContextAliasesTheSpell,
///                            SpellEffectDamageTeleportHandlers_TheTeleportPostContextAliasesTheCaster,
///                            SpellEffectDummyHandlers_TheMageContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheWarriorContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheRogueContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_TheHunterContextAliasesTheSpell,
///                            SpellEffectDummyHandlers_ThePaladinContextAliasesTheSpell
///   a rank's value changed   AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts (all 18 id -> value pairs)
///   a returned value changed SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault,
///                            SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault
///   a damage value changed   SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault,
///                            SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits
///   a stale removal mode     AuraDummyHandlers_ARemovalBodyReadsTheModeWhenItRuns

#include "TestHarness.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/AuraShapeshiftHandlers.h"
#include "spells/handlers/AuraControlHandlers.h"
#include "spells/handlers/SpellEffectTailHandlers.h"
#include "spells/handlers/AuraPeriodicHandlers.h"
#include "spells/handlers/SpellEffectHealPowerHandlers.h"
#include "spells/handlers/SpellEffectObjectCombatHandlers.h"
#include "spells/handlers/SpellTargetingHandlers.h"
#include "spells/handlers/SpellChecksHandlers.h"
#include "spells/handlers/SpellCheckTargetHandlers.h"
#include "spells/handlers/SpellEffectSkillEnchantPetHandlers.h"
#include "spells/handlers/SpellEffectDamageTeleportHandlers.h"
#include "spells/handlers/SpellEffectDummyHandlers.h"
#include "DBCStructure.h"
#include "Unit.h"                                               // SpellAuraProcResult
#include "SpellAuras.h"
#include "Spell.h"                                              // SpellCastTargets
#include "Creature.h"

#include <set>

namespace
{
    // A void site, like HandleAuraDummy's: the context holds one live-out local and a trace.
    struct VoidContext
    {
        VoidContext(int& liveOut) : value(liveOut), handled(0), defaults(0) {}

        int& value;
        uint32 handled;
        uint32 defaults;
    };

    struct VoidSiteA
    {
        static constexpr uint32 Key = 1001;
        typedef void Value;
        typedef VoidContext Context;
    };

    struct VoidSiteB
    {
        static constexpr uint32 Key = 1002;
        typedef void Value;
        typedef VoidContext Context;
    };

    // A proc site, like HandleDummyAuraProc's: the function returns a SpellAuraProcResult, which
    // its caller uses to decide the charge (Unit.cpp's proc loop).
    struct ProcContext
    {
        ProcContext(uint32& triggeredSpellId) : triggered_spell_id(triggeredSpellId) {}

        uint32& triggered_spell_id;
    };

    struct ProcSite
    {
        static constexpr uint32 Key = 1003;
        typedef SpellAuraProcResult Value;
        typedef ProcContext Context;
    };

    typedef SpellHandlerOutcome<void> VoidOutcome;
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    VoidOutcome ReturnsA(VoidContext& ctx)
    {
        ctx.handled = 1;
        return VoidOutcome::Return();
    }

    VoidOutcome ReturnsB(VoidContext& ctx)
    {
        ctx.handled = 2;
        return VoidOutcome::Return();
    }

    VoidOutcome Continues(VoidContext& ctx)
    {
        ctx.handled = 3;
        ctx.value = 42;                                         // a live-out write, as a case body's
        return VoidOutcome::Continue();
    }

    VoidOutcome DefaultBody(VoidContext& ctx)
    {
        ++ctx.defaults;
        return VoidOutcome::Continue();
    }

    ProcOutcome ProcFails(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 12345;
        return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
    }

    ProcOutcome ProcSetsTriggerAndContinues(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 54321;
        return ProcOutcome::Continue();
    }

    // The shape a void site takes where its switch stood: Return leaves the function, Continue and
    // Miss carry on after the switch. Returns whether the code after the switch ran.
    bool RunVoidSite(SpellHandlerRegistry const& registry, uint32 spellId, VoidContext& ctx)
    {
        if (registry.Dispatch<VoidSiteA>(spellId, ctx).IsReturn())
        {
            return false;
        }
        return true;
    }

    // The shape a proc site takes: Return(value) is the function's result; after the switch the
    // tail uses the live-outs, and a trigger id set by a Continue handler reaches it.
    SpellAuraProcResult RunProcSite(SpellHandlerRegistry const& registry, uint32 spellId, uint32& triggered)
    {
        ProcContext ctx(triggered);
        ProcOutcome outcome = registry.Dispatch<ProcSite>(spellId, ctx);
        if (outcome.IsReturn())
        {
            return outcome.GetValue();
        }
        return triggered ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_CANT_TRIGGER;
    }

    VoidOutcome LoopContinues(VoidContext& ctx)
    {
        ctx.handled = 4;
        return VoidOutcome::LoopContinue();
    }

    ProcOutcome ProcLoopContinues(ProcContext& ctx)
    {
        ctx.triggered_spell_id = 777;
        return ProcOutcome::LoopContinue();
    }

    // The shape a void site takes inside a loop with a statement after its switch: Return leaves the
    // function, LoopContinue takes the next id, Continue and Miss run the statement after the switch.
    // Returns how many times that statement ran.
    uint32 RunVoidSiteInALoop(SpellHandlerRegistry const& registry, uint32 const* spellIds, std::size_t count,
                              VoidContext& ctx)
    {
        uint32 after = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            VoidOutcome outcome = registry.Dispatch<VoidSiteA>(spellIds[i], ctx);
            if (outcome.IsReturn())
            {
                return after;
            }
            if (outcome.IsLoopContinue())
            {
                continue;
            }
            ++after;
        }
        return after;
    }
}

TEST(SpellHandlerRegistry_FindReturnsTheRegisteredFunction)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(200, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(registry.Find<VoidSiteA>(200) == &ReturnsB);
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));

    int local = 0;
    VoidContext ctx(local);
    CHECK(registry.Dispatch<VoidSiteA>(200, ctx).IsReturn());
    CHECK_EQ(ctx.handled, uint32(2));
}

TEST(SpellHandlerRegistry_AMissReturnsTheMissMarker)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Find<VoidSiteA>(101) == NULL);
    CHECK(registry.FindDefault<VoidSiteA>() == NULL);

    int local = 0;
    VoidContext ctx(local);
    VoidOutcome outcome = registry.Dispatch<VoidSiteA>(101, ctx);
    CHECK(outcome.IsMiss());
    CHECK(!outcome.IsReturn());
    CHECK(!outcome.IsContinue());
    CHECK_EQ(ctx.handled, uint32(0));
    CHECK(RunVoidSite(registry, 101, ctx));                     // no default: after the switch
}

TEST(SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Find<VoidSiteB>(100) == NULL);               // registering at A says nothing of B
    CHECK(registry.Register<VoidSiteB>(100, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(registry.Find<VoidSiteB>(100) == &ReturnsB);
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(1));
    CHECK_EQ(registry.CountAt(VoidSiteB::Key), std::size_t(1));

    int local = 0;
    VoidContext ctx(local);
    registry.Dispatch<VoidSiteB>(100, ctx);
    CHECK_EQ(ctx.handled, uint32(2));
    registry.Dispatch<VoidSiteA>(100, ctx);
    CHECK_EQ(ctx.handled, uint32(1));

    // Spell ids use all 32 bits (4.3.4 has ids above 65535): (A, 100 + 65536) is not (B, 100),
    // which a key packing the site into bits 16 and up would make it (1001 << 16 | 65636 ==
    // 1002 << 16 | 100).
    CHECK(registry.Register<VoidSiteA>(100 + 65536, &Continues));
    CHECK(registry.Find<VoidSiteA>(100 + 65536) == &Continues);
    CHECK(registry.Find<VoidSiteB>(100) == &ReturnsB);
    CHECK(registry.Find<VoidSiteB>(100 + 65536) == NULL);
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(VoidSiteB::Key), std::size_t(1));
}

TEST(SpellHandlerRegistry_ATakenKeyIsRefused)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(!registry.Register<VoidSiteA>(100, &ReturnsB));
    CHECK(registry.Find<VoidSiteA>(100) == &ReturnsA);
    CHECK(!registry.Register<VoidSiteA>(300, NULL));
    CHECK(registry.Find<VoidSiteA>(300) == NULL);
    CHECK(registry.RegisterDefault<VoidSiteA>(&DefaultBody));
    CHECK(!registry.RegisterDefault<VoidSiteA>(&Continues));
    CHECK(registry.FindDefault<VoidSiteA>() == &DefaultBody);
    CHECK_EQ(registry.Count(), std::size_t(1));
}

TEST(SpellHandlerRegistry_LabelsSharingABodyRegisterOneFunction)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(101, &ReturnsA));
    CHECK(registry.Find<VoidSiteA>(100) == registry.Find<VoidSiteA>(101));
    CHECK_EQ(registry.CountAt(VoidSiteA::Key), std::size_t(2));
}

TEST(SpellHandlerRegistry_ContinueAndReturnAreDistinct)
{
    CHECK(VoidOutcome::Return().IsReturn());
    CHECK(!VoidOutcome::Return().IsContinue());
    CHECK(!VoidOutcome::Return().IsMiss());
    CHECK(VoidOutcome::Continue().IsContinue());
    CHECK(!VoidOutcome::Continue().IsReturn());
    CHECK(!VoidOutcome::Continue().IsMiss());
    CHECK(VoidOutcome::Miss().IsMiss());
    CHECK(!VoidOutcome::Miss().IsReturn());
    CHECK(!VoidOutcome::Miss().IsContinue());

    CHECK(ProcOutcome::Return(SPELL_AURA_PROC_OK).IsReturn());
    CHECK(!ProcOutcome::Continue().IsReturn());
    CHECK(ProcOutcome::Continue().IsContinue());
    CHECK(!ProcOutcome::Miss().IsReturn());

    // Through a dispatch: a Continue handler carries on after the switch, a Return one leaves.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    int local = 0;
    VoidContext ctx(local);
    CHECK(!RunVoidSite(registry, 100, ctx));
    CHECK(RunVoidSite(registry, 300, ctx));
    CHECK_EQ(ctx.handled, uint32(3));
}

TEST(SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.RegisterDefault<VoidSiteA>(&DefaultBody));
    int local = 0;
    VoidContext ctx(local);

    CHECK(registry.Dispatch<VoidSiteA>(100, ctx).IsReturn());   // a hit: the default does not run
    CHECK_EQ(ctx.defaults, uint32(0));
    CHECK_EQ(ctx.handled, uint32(1));

    VoidOutcome outcome = registry.Dispatch<VoidSiteA>(999, ctx); // a miss: the default, once
    CHECK(outcome.IsContinue());
    CHECK_EQ(ctx.defaults, uint32(1));

    CHECK(registry.Dispatch<VoidSiteB>(999, ctx).IsMiss());     // another site's default is not this one's
    CHECK_EQ(ctx.defaults, uint32(1));
}

TEST(SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    int local = 7;
    VoidContext ctx(local);
    CHECK(RunVoidSite(registry, 300, ctx));
    CHECK_EQ(local, 42);

    CHECK(registry.Register<ProcSite>(400, &ProcSetsTriggerAndContinues));
    uint32 triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 400, triggered)), int(SPELL_AURA_PROC_OK));
    CHECK_EQ(triggered, uint32(54321));
}

TEST(SpellHandlerRegistry_AProcSiteReturnsItsValue)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<ProcSite>(500, &ProcFails));
    CHECK(registry.Register<ProcSite>(400, &ProcSetsTriggerAndContinues));

    // FAILED is the function's result, not replaced by what the tail would have answered: the
    // caller keeps the charge on FAILED.
    uint32 triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 500, triggered)), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(triggered, uint32(12345));

    ProcContext ctx(triggered);
    ProcOutcome outcome = registry.Dispatch<ProcSite>(500, ctx);
    CHECK(outcome.IsReturn());
    CHECK_EQ(int(outcome.GetValue()), int(SPELL_AURA_PROC_FAILED));
    CHECK_EQ(int(ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER).GetValue()), int(SPELL_AURA_PROC_CANT_TRIGGER));

    // A miss with no default reaches the tail with the live-outs as they were.
    triggered = 0;
    CHECK_EQ(int(RunProcSite(registry, 600, triggered)), int(SPELL_AURA_PROC_CANT_TRIGGER));
}

TEST(SpellHandlerRegistry_LoopContinueIsAFourthOutcome)
{
    CHECK(VoidOutcome::LoopContinue().IsLoopContinue());
    CHECK(!VoidOutcome::LoopContinue().IsReturn());
    CHECK(!VoidOutcome::LoopContinue().IsContinue());
    CHECK(!VoidOutcome::LoopContinue().IsMiss());
    CHECK(!VoidOutcome::Return().IsLoopContinue());
    CHECK(!VoidOutcome::Continue().IsLoopContinue());
    CHECK(!VoidOutcome::Miss().IsLoopContinue());

    CHECK(ProcOutcome::LoopContinue().IsLoopContinue());
    CHECK(!ProcOutcome::LoopContinue().IsReturn());
    CHECK(!ProcOutcome::LoopContinue().IsContinue());
    CHECK(!ProcOutcome::LoopContinue().IsMiss());
    CHECK(!ProcOutcome::Return(SPELL_AURA_PROC_OK).IsLoopContinue());
    CHECK(!ProcOutcome::Continue().IsLoopContinue());
    CHECK(!ProcOutcome::Miss().IsLoopContinue());

    // Through a dispatch: the handler's answer reaches the site; a miss is not a loop continue.
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(700, &LoopContinues));
    CHECK(registry.Register<ProcSite>(700, &ProcLoopContinues));
    int local = 0;
    VoidContext ctx(local);
    CHECK(registry.Dispatch<VoidSiteA>(700, ctx).IsLoopContinue());
    CHECK_EQ(ctx.handled, uint32(4));
    CHECK(!registry.Dispatch<VoidSiteA>(701, ctx).IsLoopContinue());

    uint32 triggered = 0;
    ProcContext procCtx(triggered);
    ProcOutcome outcome = registry.Dispatch<ProcSite>(700, procCtx);
    CHECK(outcome.IsLoopContinue());
    CHECK(!outcome.IsReturn());
    CHECK_EQ(triggered, uint32(777));
    CHECK(!registry.Dispatch<ProcSite>(701, procCtx).IsLoopContinue());
}

TEST(SpellHandlerRegistry_ALoopContinueSkipsTheRestOfTheLoopBody)
{
    SpellHandlerRegistry registry;
    CHECK(registry.Register<VoidSiteA>(100, &ReturnsA));
    CHECK(registry.Register<VoidSiteA>(300, &Continues));
    CHECK(registry.Register<VoidSiteA>(700, &LoopContinues));
    int local = 0;
    VoidContext ctx(local);

    static uint32 const loopContinues[] = { 700, 700 };
    CHECK_EQ(RunVoidSiteInALoop(registry, loopContinues, 2, ctx), uint32(0));
    CHECK_EQ(ctx.handled, uint32(4));

    // 300 continues and 999 misses: both run the statement; 700 skips it; 100 returns before the last 300.
    static uint32 const mixed[] = { 300, 700, 999, 100, 300 };
    CHECK_EQ(RunVoidSiteInALoop(registry, mixed, 5, ctx), uint32(2));
    CHECK_EQ(ctx.handled, uint32(1));
}

TEST(AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances)
{
    static uint32 const stances[] = { 41099, 41100, 41101, 53790, 53791, 53792 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    std::set<SpellHandler<AuraDummyApplyWarriorSite>::Function> distinct;
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyApplyWarriorSite>::Function function = registry.Find<AuraDummyApplyWarriorSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(6));                  // six bodies, none shared

    // The site has no default: a miss (Overpower 7384) is Miss() and the Overpower block runs after it.
    CHECK(registry.Find<AuraDummyApplyWarriorSite>(7384) == NULL);
    CHECK(registry.FindDefault<AuraDummyApplyWarriorSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyApplyWarriorSite>(7384, ctx).IsMiss());

    // The game's table is the same one.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    for (uint32 spellId : stances)
    {
        CHECK(game.Find<AuraDummyApplyWarriorSite>(spellId) == registry.Find<AuraDummyApplyWarriorSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = reinterpret_cast<Aura*>(units[1]);
    AuraDummyApplyContext ctx(aura, target);
    CHECK(ctx.aura == aura);
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

namespace
{
    alignas(16) unsigned char s_auraStorage[3][16];

    Aura* FakeAura(int i)
    {
        return reinterpret_cast<Aura*>(s_auraStorage[i]);
    }

    uint32 const QUEST_TAME_RETURNED = 0xFFFFFFFF;

    // The quest-tame site's shape: answers what the tail casts (0: nothing), or the marker when the site returned.
    uint32 RunQuestTame(SpellHandlerRegistry const& registry, uint32 tameId)
    {
        uint32 finalSpellId = 0;
        AuraDummyQuestTameContext tameCtx(finalSpellId);
        if (registry.Dispatch<AuraDummyQuestTameSite>(tameId, tameCtx).IsReturn())
        {
            return QUEST_TAME_RETURNED;
        }
        return finalSpellId;
    }
}

TEST(AuraDummyHandlers_TheTableRegistersEveryRowOnce)
{
    // 6 warrior stances, 18 quest-tame labels, 30 removal labels, 16 feign-death labels, 2 druid labels: 72 rows
    // and 72 keys, none twice, and no default.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(72));
    CHECK_EQ(registry.Count(), std::size_t(72));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraDummyRemoveSite::Key), std::size_t(30));
    CHECK_EQ(registry.CountAt(AuraDummyQuestTameSite::Key), std::size_t(18));
    CHECK_EQ(registry.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(16));
    CHECK_EQ(registry.CountAt(AuraDummyDruidSite::Key), std::size_t(2));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(72));
    CHECK_EQ(registry.Count(), std::size_t(72));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));

    // The game's table holds these, the transform site's 9 rows and default, the threat site's 2 rows, the
    // transmitted-object site's row and default, the periodic auras' 26 rows and 4 defaults, the energize site's
    // 9 rows and default, the activate-object site's 33 rows, the resurrect site's 3 rows and default, the two
    // targeting sites' 10 and 1 rows and their two defaults, the cast-check site's 2 rows and default, the
    // target-check site's 3 rows and default, the weapon-damage site's 6 rows, the school-damage, trigger-spell
    // and two teleport sites' 74, 7, 3 and 3 rows, and the five dummy-effect family sites' 5, 11, 4, 4 and 4 rows.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(292));
    CHECK_EQ(game.CountDefaults(), std::size_t(12));
    CHECK_EQ(game.CountAt(AuraDummyRemoveSite::Key), std::size_t(30));
}

TEST(AuraDummyHandlers_TheDruidSiteHoldsItsTwoLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    SpellHandler<AuraDummyDruidSite>::Function savageRoar = registry.Find<AuraDummyDruidSite>(52610);
    SpellHandler<AuraDummyDruidSite>::Function survivalInstincts = registry.Find<AuraDummyDruidSite>(61336);
    CHECK(savageRoar != NULL);
    CHECK(survivalInstincts != NULL);
    CHECK(savageRoar != survivalInstincts);
    CHECK(registry.FindDefault<AuraDummyDruidSite>() == NULL);

    // An unmatched druid id is a Miss: HandleAuraDummy goes on to the Lifebloom and Predatory Strikes
    // blocks after the switch (Lifebloom 33763, Predatory Strikes 16972).
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyDruidSite>(33763, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyDruidSite>(16972, ctx).IsMiss());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraDummyDruidSite>(52610) == savageRoar);
    CHECK(game.Find<AuraDummyDruidSite>(61336) == survivalInstincts);
}

TEST(AuraDummyHandlers_TheApplyRemoveContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = FakeAura(2);

    AuraDummyApplyRemoveContext applyRemove(aura, target, false);
    CHECK(applyRemove.aura == aura);
    CHECK(applyRemove.target == target);
    CHECK(!applyRemove.apply);
    applyRemove.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
}

TEST(AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyLabelsAndNoDefault)
{
    static uint32 const stances[] = { 41099, 41100, 41101, 53790, 53791, 53792 };
    static uint32 const byMode[] = { 12774, 32045, 32051, 32052, 32286, 42454,
                                     43969, 45934, 50141, 51870, 52098, 56511, 61900 };
    static uint32 const byBody[] = { 10255, 12479, 28169, 35079, 36730, 45963, 46308, 46637, 53039, 68839 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    CHECK_EQ(registry.CountAt(AuraDummyRemoveSite::Key), std::size_t(30));
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);

        // The same id at the warrior apply site is another key with another body.
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) != NULL);
        CHECK(reinterpret_cast<void (*)()>(registry.Find<AuraDummyApplyWarriorSite>(spellId))
              != reinterpret_cast<void (*)()>(function));
    }

    // The thirteen labels whose bodies read the removal mode: thirteen bodies, none shared, and none a stance's.
    std::set<SpellHandler<AuraDummyRemoveSite>::Function> distinct;
    for (uint32 spellId : byMode)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(13));
    for (uint32 spellId : stances)
    {
        CHECK(distinct.count(registry.Find<AuraDummyRemoveSite>(spellId)) == 0);
    }

    // The eleven labels whose bodies read no mode and need no motion or map header: ten bodies, 35079 and 59628
    // one function; none a stance's or a mode reader's.
    std::set<SpellHandler<AuraDummyRemoveSite>::Function> others;
    for (uint32 spellId : byBody)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);
        CHECK(distinct.count(function) == 0);
        others.insert(function);
    }
    CHECK_EQ(others.size(), std::size_t(10));
    // 35079 and 59628 one function: proven by verbatim, not here: this binary links with COMDAT folding, so an
    // identical second function folds to the same address.
    CHECK(registry.Find<AuraDummyRemoveSite>(59628) == registry.Find<AuraDummyRemoveSite>(35079));
    for (uint32 spellId : stances)
    {
        CHECK(others.count(registry.Find<AuraDummyRemoveSite>(spellId)) == 0);
    }

    // No default: an id the switch still holds (42517, 44191, 48385, 51405) or none misses, and that switch runs.
    CHECK(registry.FindDefault<AuraDummyRemoveSite>() == NULL);
    Unit* target = NULL;
    AuraDummyRemoveContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(42517, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(44191, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(48385, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(51405, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(11920, ctx).IsMiss());

    // Keyed on the removal switch only: the quest-tame and feign-death sites do not hold its labels.
    CHECK(registry.Find<AuraDummyQuestTameSite>(32045) == NULL);
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(61900) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyRemoveSite::Key), std::size_t(30));
    for (uint32 spellId : stances)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    for (uint32 spellId : byMode)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    for (uint32 spellId : byBody)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
    CHECK(game.Find<AuraDummyRemoveSite>(59628) == registry.Find<AuraDummyRemoveSite>(59628));
}

namespace
{
    // An Aura's storage without its constructor (which needs the spell store): SetRemoveMode and GetRemoveMode
    // touch only the mode's bits, and a body that returns on the mode reads nothing else of the aura.
    struct AuraStorage
    {
        alignas(Aura) unsigned char bytes[sizeof(Aura)];
    };

    AuraStorage s_modeAura = {};

    Aura* ModeAura()
    {
        return reinterpret_cast<Aura*>(s_modeAura.bytes);
    }

    AuraRemoveMode s_modeTheBodyRead = AURA_REMOVE_BY_DEFAULT;
}

TEST(AuraDummyHandlers_GetRemoveModeAnswersTheModeSetRemoveModeWrote)
{
    static AuraRemoveMode const modes[] =
    {
        AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_STACK, AURA_REMOVE_BY_CANCEL, AURA_REMOVE_BY_DISPEL,
        AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DELETE, AURA_REMOVE_BY_SHIELD_BREAK, AURA_REMOVE_BY_EXPIRE,
        AURA_REMOVE_BY_TRACKING,
    };
    Aura* aura = ModeAura();
    for (AuraRemoveMode mode : modes)
    {
        aura->SetRemoveMode(mode);                              // Unit::RemoveAura's write
        CHECK_EQ(int(aura->GetRemoveMode()), int(mode));
    }
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
}

TEST(AuraDummyHandlers_ARemovalBodyReadsTheModeWhenItRuns)
{
    // A double at the site: the context carries the aura, and the body reads the mode set after the context was
    // built, before the body ran.
    SpellHandlerRegistry doubled;
    CHECK(doubled.Register<AuraDummyRemoveSite>(32045, [](AuraDummyRemoveContext& ctx)
    {
        s_modeTheBodyRead = ctx.aura->GetRemoveMode();
        return SpellHandlerOutcome<void>::Return();
    }));
    Aura* aura = ModeAura();
    Unit* target = NULL;
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    AuraDummyRemoveContext doubleCtx(aura, target);
    aura->SetRemoveMode(AURA_REMOVE_BY_EXPIRE);
    CHECK(doubled.Dispatch<AuraDummyRemoveSite>(32045, doubleCtx).IsReturn());
    CHECK_EQ(int(s_modeTheBodyRead), int(AURA_REMOVE_BY_EXPIRE));

    // The real bodies: each context is built while the aura holds the mode its body acts on (a cast on the
    // target, a read of the caster); the mode then changes to one the body returns on before touching anything.
    // A body that read the mode when its context was built would act on the zeroed aura or the NULL target and crash.
    struct ModeBody
    {
        uint32 spellId;
        AuraRemoveMode acts;
        AuraRemoveMode returns;
    };
    static ModeBody const bodies[] =
    {
        { 12774, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_DEATH },
        { 32045, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32051, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32052, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 32286, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 42454, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE },
        { 43969, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 45934, AURA_REMOVE_BY_DISPEL, AURA_REMOVE_BY_DEFAULT },
        { 50141, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 52098, AURA_REMOVE_BY_EXPIRE, AURA_REMOVE_BY_DEFAULT },
        { 56511, AURA_REMOVE_BY_DEFAULT, AURA_REMOVE_BY_EXPIRE },
        { 61900, AURA_REMOVE_BY_DEATH, AURA_REMOVE_BY_DEFAULT },
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (ModeBody const& body : bodies)
    {
        aura->SetRemoveMode(body.acts);
        AuraDummyRemoveContext ctx(aura, target);
        aura->SetRemoveMode(body.returns);
        CHECK(game.Dispatch<AuraDummyRemoveSite>(body.spellId, ctx).IsReturn());
    }
    CHECK(target == NULL);
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
}

TEST(AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    Aura* aura = FakeAura(1);
    AuraDummyRemoveContext ctx(aura, target);
    CHECK(ctx.aura == aura);
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

TEST(AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault)
{
    static uint32 const tames[] = { 19548, 19674, 19687, 19688, 19689, 19692, 19693, 19694, 19696,
                                    19697, 19699, 19700, 30646, 30653, 30654, 30099, 30102, 30105 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    for (uint32 spellId : tames)
    {
        CHECK(registry.Find<AuraDummyQuestTameSite>(spellId) != NULL);
    }

    // No default: a quest-tame id the switch never held (73461), another site's id or none is a Miss.
    CHECK(registry.FindDefault<AuraDummyQuestTameSite>() == NULL);
    uint32 finalSpellId = 0;
    AuraDummyQuestTameContext ctx(finalSpellId);
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(73461, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(41101, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyQuestTameSite>(12345, ctx).IsMiss());
    CHECK_EQ(finalSpellId, uint32(0));
    CHECK(registry.Find<AuraDummyRemoveSite>(19548) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyQuestTameSite::Key), std::size_t(18));
    for (uint32 spellId : tames)
    {
        CHECK(game.Find<AuraDummyQuestTameSite>(spellId) == registry.Find<AuraDummyQuestTameSite>(spellId));
    }
}

TEST(AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts)
{
    // Each body writes `finalSpellId` and answers Continue, so the tail casts it; a miss leaves it at 0.
    static uint32 const tames[][2] =
    {
        { 19548, 19597 }, { 19674, 19677 }, { 19687, 19676 }, { 19688, 19678 }, { 19689, 19679 }, { 19692, 19680 },
        { 19693, 19684 }, { 19694, 19681 }, { 19696, 19682 }, { 19697, 19683 }, { 19699, 19685 }, { 19700, 19686 },
        { 30646, 30647 }, { 30653, 30648 }, { 30654, 30652 }, { 30099, 30100 }, { 30102, 30103 }, { 30105, 30104 },
    };
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (auto const& tame : tames)
    {
        CHECK_EQ(RunQuestTame(game, tame[0]), tame[1]);
    }
    CHECK_EQ(RunQuestTame(game, 73461), uint32(0));

    // Against a Return handler at the same site, which leaves before the tail.
    SpellHandlerRegistry returning;
    CHECK(returning.Register<AuraDummyQuestTameSite>(19548, [](AuraDummyQuestTameContext& ctx)
    {
        ctx.finalSpellId = 19597;
        return SpellHandlerOutcome<void>::Return();
    }));
    CHECK_EQ(RunQuestTame(returning, 19548), QUEST_TAME_RETURNED);
}

TEST(AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId)
{
    uint32 finalSpellId = 0;
    AuraDummyQuestTameContext ctx(finalSpellId);
    ctx.finalSpellId = 19684;                                   // a body's write to `finalSpellId`...
    CHECK_EQ(finalSpellId, uint32(19684));                      // ...is the block's local
    finalSpellId = 30647;
    CHECK_EQ(ctx.finalSpellId, uint32(30647));
}

TEST(AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels)
{
    static uint32 const feigns[] = { 29266, 31261, 37493, 52593, 55795, 57626, 57685, 58768,
                                     58806, 58951, 64461, 65985, 70592, 70628, 70630, 71598 };

    // The sixteen labels share one body: one function under sixteen keys.
    // Proven by verbatim, not here: this binary links with COMDAT folding, so an identical second function folds
    // to the same address.
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    SpellHandler<AuraDummyApplyRemoveGenericSite>::Function feignDeath =
        registry.Find<AuraDummyApplyRemoveGenericSite>(29266);
    CHECK(feignDeath != NULL);
    for (uint32 spellId : feigns)
    {
        CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) == feignDeath);
    }
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(31261)
          == registry.Find<AuraDummyApplyRemoveGenericSite>(71598));
    CHECK_EQ(registry.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(16));

    // No default: a label the switch still holds (the spawn feign deaths 35356 and 51329, 6606) or none is a Miss,
    // and that switch runs.
    CHECK(registry.FindDefault<AuraDummyApplyRemoveGenericSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(35356, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(51329, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(6606, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyApplyRemoveGenericSite>(12345, ctx).IsMiss());

    // Keyed on the generic switch only: the druid labels are not its keys, and its labels are no other site's.
    CHECK(registry.Find<AuraDummyApplyRemoveGenericSite>(52610) == NULL);
    CHECK(registry.Find<AuraDummyDruidSite>(29266) == NULL);
    CHECK(registry.Find<AuraDummyRemoveSite>(29266) == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(16));
    for (uint32 spellId : feigns)
    {
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == feignDeath);
    }
}

TEST(AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault)
{
    static uint32 const labels[] = { 16739, 42365, 50517, 51926, 65386, 65495, 65528, 65529, 71450 };

    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraShapeshiftHandlers(registry), uint32(10)); // nine rows and the default
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(AuraTransformSite::Key), std::size_t(9));

    std::set<SpellHandler<AuraTransformSite>::Function> distinct;
    for (uint32 spellId : labels)
    {
        SpellHandler<AuraTransformSite>::Function function = registry.Find<AuraTransformSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    // Seven bodies: 50517 and 51926 share one, 65386 and 65495 another. Proven by verbatim, not here: this binary
    // links with COMDAT folding, so an identical second function folds to the same address.
    CHECK_EQ(distinct.size(), std::size_t(7));
    CHECK(registry.Find<AuraTransformSite>(50517) == registry.Find<AuraTransformSite>(51926));
    CHECK(registry.Find<AuraTransformSite>(65386) == registry.Find<AuraTransformSite>(65495));

    // The default is its own body; an id with no row finds none.
    SpellHandler<AuraTransformSite>::Function onMiss = registry.FindDefault<AuraTransformSite>();
    CHECK(onMiss != NULL);
    CHECK(distinct.count(onMiss) == 0);
    CHECK(registry.Find<AuraTransformSite>(44186) == NULL);     // commented out in the switch: the default's
    CHECK(registry.Find<AuraTransformSite>(12345) == NULL);

    // Registering again on the same table changes nothing: every key and the default are taken.
    CHECK_EQ(RegisterAuraShapeshiftHandlers(registry), uint32(10));
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.FindDefault<AuraTransformSite>() == onMiss);

    // Keyed on the transform site only: its labels are no aura dummy site's, and theirs are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == NULL);
    }
    CHECK(game.Find<AuraTransformSite>(41099) == NULL);
    CHECK(game.Find<AuraTransformSite>(29266) == NULL);

    // The game's table holds the same rows and the same default.
    CHECK_EQ(game.CountAt(AuraTransformSite::Key), std::size_t(9));
    for (uint32 spellId : labels)
    {
        CHECK(game.Find<AuraTransformSite>(spellId) == registry.Find<AuraTransformSite>(spellId));
    }
    CHECK(game.FindDefault<AuraTransformSite>() == onMiss);
}

TEST(AuraShapeshiftHandlers_TheTransformContextAliasesTheTargetLocal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraTransformContext ctx(65528, target);
    CHECK_EQ(ctx.spellId, uint32(65528));
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);             // a body's write to `target`...
    CHECK(target == reinterpret_cast<Unit*>(units[1]));         // ...is the function's local
}

TEST(AuraControlHandlers_TheThreatSiteHoldsItsTwoLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraControlHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(AuraThreatSite::Key), std::size_t(2));

    // Two labels, two distinct bodies.
    SpellHandler<AuraThreatSite>::Function arcaneShroud = registry.Find<AuraThreatSite>(26400);
    SpellHandler<AuraThreatSite>::Function eyeOfDiminution = registry.Find<AuraThreatSite>(28862);
    CHECK(arcaneShroud != NULL);
    CHECK(eyeOfDiminution != NULL);
    CHECK(arcaneShroud != eyeOfDiminution);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the member goes on.
    CHECK(registry.FindDefault<AuraThreatSite>() == NULL);
    CHECK(registry.Find<AuraThreatSite>(12345) == NULL);
    Unit* target = NULL;
    int level_diff = 0;
    int multiplier = 0;
    AuraThreatContext ctx(target, level_diff, multiplier);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraThreatSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK_EQ(level_diff, 0);
    CHECK_EQ(multiplier, 0);

    // Registering again on the same table changes nothing: both keys are taken.
    CHECK_EQ(RegisterAuraControlHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK(registry.Find<AuraThreatSite>(26400) == arcaneShroud);
    CHECK(registry.Find<AuraThreatSite>(28862) == eyeOfDiminution);

    // Keyed on the threat site only: its labels are no other site's, and the other sites' are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 26400u, 28862u })
    {
        CHECK(game.Find<AuraTransformSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(spellId) == NULL);
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == NULL);
    }
    CHECK(game.Find<AuraThreatSite>(16739) == NULL);
    CHECK(game.Find<AuraThreatSite>(29266) == NULL);

    // The game's table holds the same rows and no default.
    CHECK_EQ(game.CountAt(AuraThreatSite::Key), std::size_t(2));
    CHECK(game.Find<AuraThreatSite>(26400) == arcaneShroud);
    CHECK(game.Find<AuraThreatSite>(28862) == eyeOfDiminution);
    CHECK(game.FindDefault<AuraThreatSite>() == NULL);
}

TEST(AuraControlHandlers_TheThreatContextAliasesTheThreeLocals)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    int level_diff = 0;
    int multiplier = 0;
    AuraThreatContext ctx(target, level_diff, multiplier);
    CHECK(ctx.target == target);
    ctx.level_diff = 13;                                        // a body's writes to the two locals...
    ctx.multiplier = 2;
    CHECK_EQ(level_diff, 13);                                   // ...are the function's locals, which it reads after
    CHECK_EQ(multiplier, 2);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    level_diff = -5;
    CHECK_EQ(ctx.level_diff, -5);
}

TEST(SpellEffectTailHandlers_TheTransmittedSiteHoldsItsLabelAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectTailHandlers(registry), uint32(2)); // one row and the default
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectTransmittedSite::Key), std::size_t(1));

    // One label with its own body, and the default, another body.
    SpellHandler<SpellEffectTransmittedSite>::Function createSoulwell =
        registry.Find<SpellEffectTransmittedSite>(29886);
    SpellHandler<SpellEffectTransmittedSite>::Function onMiss = registry.FindDefault<SpellEffectTransmittedSite>();
    CHECK(createSoulwell != NULL);
    CHECK(onMiss != NULL);
    CHECK(createSoulwell != onMiss);
    CHECK(registry.Find<SpellEffectTransmittedSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and keeps the
    // effect's entry.
    Unit* caster = NULL;
    uint32 name_id = 177000;
    SpellEffectTransmittedContext ctx(caster, name_id);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectTransmittedSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(name_id, uint32(177000));
    CHECK(caster == NULL);

    // Registering again on the same table changes nothing: the key and the default are taken.
    CHECK_EQ(RegisterSpellEffectTailHandlers(registry), uint32(2));
    CHECK_EQ(registry.Count(), std::size_t(1));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectTransmittedSite>(29886) == createSoulwell);
    CHECK(registry.FindDefault<SpellEffectTransmittedSite>() == onMiss);

    // Keyed on the transmitted-object site only: its label is no other site's, and the other sites' are not its.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraTransformSite>(29886) == NULL);
    CHECK(game.Find<AuraThreatSite>(29886) == NULL);
    CHECK(game.Find<AuraDummyApplyRemoveGenericSite>(29886) == NULL);
    CHECK(game.Find<AuraDummyRemoveSite>(29886) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(16739) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(26400) == NULL);
    CHECK(game.Find<SpellEffectTransmittedSite>(29266) == NULL);

    // The game's table holds the same row and the same default.
    CHECK_EQ(game.CountAt(SpellEffectTransmittedSite::Key), std::size_t(1));
    CHECK(game.Find<SpellEffectTransmittedSite>(29886) == createSoulwell);
    CHECK(game.FindDefault<SpellEffectTransmittedSite>() == onMiss);
}

TEST(SpellEffectTailHandlers_TheTransmittedContextAliasesTheCasterAndTheEntry)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    uint32 name_id = 177000;
    SpellEffectTransmittedContext ctx(caster, name_id);
    CHECK(ctx.m_caster == caster);
    CHECK_EQ(ctx.name_id, uint32(177000));
    ctx.name_id = 183510;                                       // a body's write to `name_id`...
    CHECK_EQ(name_id, uint32(183510));                          // ...is the function's local, which it reads after
    name_id = 183511;
    CHECK_EQ(ctx.name_id, uint32(183511));
    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

namespace
{
    // Whether `spellId` is a key at a site other than `site`: the periodic auras' five, the energize, activate-object,
    // resurrect, two targeting, cast-check, target-check, weapon-damage, school-damage, trigger-spell and two teleport
    // sites and the earlier files' ids.
    bool HeldByAnotherSite(SpellHandlerRegistry const& registry, uint32 site, uint32 spellId)
    {
        return (site != AuraProcTriggerSite::Key && registry.Find<AuraProcTriggerSite>(spellId) != NULL) ||
               (site != AuraPeriodicTriggerSite::Key && registry.Find<AuraPeriodicTriggerSite>(spellId) != NULL) ||
               (site != AuraPeriodicEnergizeSite::Key && registry.Find<AuraPeriodicEnergizeSite>(spellId) != NULL) ||
               (site != AuraPeriodicDummyRogueSite::Key &&
                registry.Find<AuraPeriodicDummyRogueSite>(spellId) != NULL) ||
               (site != AuraIncreaseHealthSite::Key && registry.Find<AuraIncreaseHealthSite>(spellId) != NULL) ||
               (site != SpellEffectEnergizeSite::Key && registry.Find<SpellEffectEnergizeSite>(spellId) != NULL) ||
               (site != SpellEffectActivateObjectSite::Key &&
                registry.Find<SpellEffectActivateObjectSite>(spellId) != NULL) ||
               (site != SpellEffectResurrectSite::Key && registry.Find<SpellEffectResurrectSite>(spellId) != NULL) ||
               (site != SpellTargetAllEnemyInAreaSite::Key &&
                registry.Find<SpellTargetAllEnemyInAreaSite>(spellId) != NULL) ||
               (site != SpellTargetEffectDummySite::Key &&
                registry.Find<SpellTargetEffectDummySite>(spellId) != NULL) ||
               (site != SpellCheckCastAuraDummySite::Key &&
                registry.Find<SpellCheckCastAuraDummySite>(spellId) != NULL) ||
               (site != SpellCheckTargetSite::Key && registry.Find<SpellCheckTargetSite>(spellId) != NULL) ||
               (site != SpellEffectWeaponDmgSite::Key && registry.Find<SpellEffectWeaponDmgSite>(spellId) != NULL) ||
               (site != SpellEffectSchoolDmgSite::Key && registry.Find<SpellEffectSchoolDmgSite>(spellId) != NULL) ||
               (site != SpellEffectTriggerSpellSite::Key &&
                registry.Find<SpellEffectTriggerSpellSite>(spellId) != NULL) ||
               (site != SpellEffectTeleportRecallSite::Key &&
                registry.Find<SpellEffectTeleportRecallSite>(spellId) != NULL) ||
               (site != SpellEffectTeleportPostSite::Key &&
                registry.Find<SpellEffectTeleportPostSite>(spellId) != NULL) ||
               (site != SpellEffectDummyMageSite::Key && registry.Find<SpellEffectDummyMageSite>(spellId) != NULL) ||
               (site != SpellEffectDummyWarriorSite::Key &&
                registry.Find<SpellEffectDummyWarriorSite>(spellId) != NULL) ||
               (site != SpellEffectDummyRogueSite::Key && registry.Find<SpellEffectDummyRogueSite>(spellId) != NULL) ||
               (site != SpellEffectDummyHunterSite::Key &&
                registry.Find<SpellEffectDummyHunterSite>(spellId) != NULL) ||
               (site != SpellEffectDummyPaladinSite::Key &&
                registry.Find<SpellEffectDummyPaladinSite>(spellId) != NULL) ||
               registry.Find<AuraTransformSite>(spellId) != NULL || registry.Find<AuraThreatSite>(spellId) != NULL ||
               registry.Find<SpellEffectTransmittedSite>(spellId) != NULL ||
               registry.Find<AuraDummyRemoveSite>(spellId) != NULL ||
               registry.Find<AuraDummyApplyRemoveGenericSite>(spellId) != NULL;
    }

    // 2 proc-trigger labels, 4 periodic-trigger labels, 5 energize labels, 1 rogue label and 14 health labels: 26
    // rows; four of the five sites have a `default:`.
    uint32 const PERIODIC_ROWS = 26;
    uint32 const PERIODIC_DEFAULTS = 4;

    // Registers the file's handlers on an empty `registry`, checks the counts, and that a second registration
    // changes none of them.
    void RegisterPeriodicTwice(SpellHandlerRegistry& registry)
    {
        CHECK_EQ(RegisterAuraPeriodicHandlers(registry), PERIODIC_ROWS + PERIODIC_DEFAULTS);
        CHECK_EQ(registry.Count(), std::size_t(PERIODIC_ROWS));
        CHECK_EQ(registry.CountDefaults(), std::size_t(PERIODIC_DEFAULTS));
        CHECK_EQ(RegisterAuraPeriodicHandlers(registry), PERIODIC_ROWS + PERIODIC_DEFAULTS);
        CHECK_EQ(registry.Count(), std::size_t(PERIODIC_ROWS));
        CHECK_EQ(registry.CountDefaults(), std::size_t(PERIODIC_DEFAULTS));
    }
}

TEST(AuraPeriodicHandlers_TheProcTriggerSiteHoldsItsTwoLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraProcTriggerSite::Key), std::size_t(2));

    // Two labels, two distinct bodies, and a default that is neither.
    SpellHandler<AuraProcTriggerSite>::Function ascendance = registry.Find<AuraProcTriggerSite>(28200);
    SpellHandler<AuraProcTriggerSite>::Function vigilance = registry.Find<AuraProcTriggerSite>(50720);
    SpellHandler<AuraProcTriggerSite>::Function onMiss = registry.FindDefault<AuraProcTriggerSite>();
    CHECK(ascendance != NULL);
    CHECK(vigilance != NULL);
    CHECK(onMiss != NULL);
    CHECK(ascendance != vigilance);
    CHECK(onMiss != ascendance && onMiss != vigilance);
    CHECK(registry.Find<AuraProcTriggerSite>(12345) == NULL);

    // The switch's `default: break;`: a miss runs the default, which continues after the switch; 28200 at remove
    // reads only `apply` and continues too.
    Unit* target = NULL;
    bool apply = false;
    AuraProcTriggerContext ctx(ModeAura(), target, apply);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraProcTriggerSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    CHECK(registry.Dispatch<AuraProcTriggerSite>(28200, ctx).IsContinue());
    CHECK(target == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 28200u, 50720u })
    {
        CHECK(!HeldByAnotherSite(game, AuraProcTriggerSite::Key, spellId));
    }
    CHECK_EQ(game.CountAt(AuraProcTriggerSite::Key), std::size_t(2));
    CHECK(game.Find<AuraProcTriggerSite>(28200) == ascendance);
    CHECK(game.Find<AuraProcTriggerSite>(50720) == vigilance);
    CHECK(game.FindDefault<AuraProcTriggerSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheProcTriggerContextAliasesTheTargetAndApply)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = true;
    AuraProcTriggerContext ctx(ModeAura(), target, apply);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(ctx.apply);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = false;
    CHECK(!ctx.apply);
}

TEST(AuraPeriodicHandlers_ThePeriodicTriggerSiteHoldsItsFourLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicTriggerSite::Key), std::size_t(4));

    // Four labels, four distinct bodies, and a default that is none of them.
    static uint32 const ids[] = { 66, 42783, 46221, 51912 };
    std::set<SpellHandler<AuraPeriodicTriggerSite>::Function> bodies;
    for (uint32 spellId : ids)
    {
        SpellHandler<AuraPeriodicTriggerSite>::Function body = registry.Find<AuraPeriodicTriggerSite>(spellId);
        CHECK(body != NULL);
        bodies.insert(body);
    }
    CHECK_EQ(bodies.size(), std::size_t(4));
    SpellHandler<AuraPeriodicTriggerSite>::Function onMiss = registry.FindDefault<AuraPeriodicTriggerSite>();
    CHECK(onMiss != NULL);
    CHECK(bodies.count(onMiss) == 0);
    CHECK(registry.Find<AuraPeriodicTriggerSite>(12345) == NULL);

    // A miss runs the `default: break;` and continues after the switch. 66, 42783 and 51912 act only at expiry: at
    // another removal mode they read the mode alone and return from the member at their `return;`.
    Aura* aura = ModeAura();
    Unit* target = NULL;
    aura->SetRemoveMode(AURA_REMOVE_BY_DEFAULT);
    AuraPeriodicTriggerContext ctx(aura, target);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicTriggerSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    for (uint32 spellId : { 66u, 42783u, 51912u })
    {
        CHECK(registry.Dispatch<AuraPeriodicTriggerSite>(spellId, ctx).IsReturn());
    }
    CHECK(target == NULL);

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ids)
    {
        CHECK(!HeldByAnotherSite(game, AuraPeriodicTriggerSite::Key, spellId));
        CHECK(game.Find<AuraPeriodicTriggerSite>(spellId) == registry.Find<AuraPeriodicTriggerSite>(spellId));
    }
    CHECK_EQ(game.CountAt(AuraPeriodicTriggerSite::Key), std::size_t(4));
    CHECK(game.FindDefault<AuraPeriodicTriggerSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_ThePeriodicTriggerContextAliasesTheTarget)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraPeriodicTriggerContext ctx(ModeAura(), target);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = NULL;
    CHECK(ctx.target == NULL);
}

TEST(AuraPeriodicHandlers_TheEnergizeSiteHoldsItsFiveLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicEnergizeSite::Key), std::size_t(5));

    // Five labels, four bodies (the two Replenishment labels share one), and a default that is none of them.
    SpellHandler<AuraPeriodicEnergizeSite>::Function glyph = registry.Find<AuraPeriodicEnergizeSite>(54833);
    SpellHandler<AuraPeriodicEnergizeSite>::Function innervate = registry.Find<AuraPeriodicEnergizeSite>(29166);
    SpellHandler<AuraPeriodicEnergizeSite>::Function owlkin = registry.Find<AuraPeriodicEnergizeSite>(48391);
    SpellHandler<AuraPeriodicEnergizeSite>::Function replenishment = registry.Find<AuraPeriodicEnergizeSite>(57669);
    SpellHandler<AuraPeriodicEnergizeSite>::Function onMiss = registry.FindDefault<AuraPeriodicEnergizeSite>();
    std::set<SpellHandler<AuraPeriodicEnergizeSite>::Function> bodies = { glyph, innervate, owlkin, replenishment,
                                                                          onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));
    CHECK(registry.Find<AuraPeriodicEnergizeSite>(61782) == replenishment);
    CHECK(registry.Find<AuraPeriodicEnergizeSite>(12345) == NULL);

    // A miss runs the `default: break;` and continues after the switch.
    Unit* target = NULL;
    AuraPeriodicEnergizeContext ctx(ModeAura(), target);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicEnergizeSite>(12345, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 54833u, 29166u, 48391u, 57669u, 61782u })
    {
        CHECK(!HeldByAnotherSite(game, AuraPeriodicEnergizeSite::Key, spellId));
        CHECK(game.Find<AuraPeriodicEnergizeSite>(spellId) == registry.Find<AuraPeriodicEnergizeSite>(spellId));
    }
    CHECK_EQ(game.CountAt(AuraPeriodicEnergizeSite::Key), std::size_t(5));
    CHECK(game.FindDefault<AuraPeriodicEnergizeSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheEnergizeContextAliasesTheTarget)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    AuraPeriodicEnergizeContext ctx(ModeAura(), target);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = NULL;
    CHECK(ctx.target == NULL);
}

TEST(AuraPeriodicHandlers_TheRogueSiteHoldsItsOneLabelAndNoDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraPeriodicDummyRogueSite::Key), std::size_t(1));
    SpellHandler<AuraPeriodicDummyRogueSite>::Function masterOfSubtlety =
        registry.Find<AuraPeriodicDummyRogueSite>(31666);
    CHECK(masterOfSubtlety != NULL);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family case goes on.
    CHECK(registry.FindDefault<AuraPeriodicDummyRogueSite>() == NULL);
    CHECK(registry.Find<AuraPeriodicDummyRogueSite>(31665) == NULL);
    Unit* target = NULL;
    bool apply = true;
    AuraPeriodicDummyRogueContext ctx(ModeAura(), target, apply);
    SpellHandlerOutcome<void> miss = registry.Dispatch<AuraPeriodicDummyRogueSite>(31665, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(!HeldByAnotherSite(game, AuraPeriodicDummyRogueSite::Key, 31666));
    CHECK_EQ(game.CountAt(AuraPeriodicDummyRogueSite::Key), std::size_t(1));
    CHECK(game.Find<AuraPeriodicDummyRogueSite>(31666) == masterOfSubtlety);
    CHECK(game.FindDefault<AuraPeriodicDummyRogueSite>() == NULL);
}

TEST(AuraPeriodicHandlers_TheRogueContextAliasesTheTargetAndApply)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = false;
    AuraPeriodicDummyRogueContext ctx(ModeAura(), target, apply);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(!ctx.apply);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = true;
    CHECK(ctx.apply);
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthSiteHoldsItsFourteenLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    RegisterPeriodicTwice(registry);
    CHECK_EQ(registry.CountAt(AuraIncreaseHealthSite::Key), std::size_t(14));

    // Fourteen labels, two bodies and a default: the three percentage labels share one body, the eleven flat
    // labels the other.
    static uint32 const percent[] = { 54443, 55233, 61254 };
    static uint32 const flat[] = { 12976, 28726, 31616, 34511, 44055, 55915, 55917, 67596, 50322, 53479, 59465 };
    SpellHandler<AuraIncreaseHealthSite>::Function percentBody = registry.Find<AuraIncreaseHealthSite>(54443);
    SpellHandler<AuraIncreaseHealthSite>::Function flatBody = registry.Find<AuraIncreaseHealthSite>(12976);
    SpellHandler<AuraIncreaseHealthSite>::Function onMiss = registry.FindDefault<AuraIncreaseHealthSite>();
    CHECK(percentBody != NULL);
    CHECK(flatBody != NULL);
    CHECK(onMiss != NULL);
    CHECK(percentBody != flatBody);
    CHECK(onMiss != percentBody && onMiss != flatBody);
    CHECK(registry.Find<AuraIncreaseHealthSite>(60430) == NULL);   // HandleAuraModIncreaseHealthPercent's, a lookup

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : percent)
    {
        CHECK(registry.Find<AuraIncreaseHealthSite>(spellId) == percentBody);
        CHECK(game.Find<AuraIncreaseHealthSite>(spellId) == percentBody);
        CHECK(!HeldByAnotherSite(game, AuraIncreaseHealthSite::Key, spellId));
    }
    for (uint32 spellId : flat)
    {
        CHECK(registry.Find<AuraIncreaseHealthSite>(spellId) == flatBody);
        CHECK(game.Find<AuraIncreaseHealthSite>(spellId) == flatBody);
        CHECK(!HeldByAnotherSite(game, AuraIncreaseHealthSite::Key, spellId));
    }
    CHECK_EQ(game.CountAt(AuraIncreaseHealthSite::Key), std::size_t(14));
    CHECK(game.FindDefault<AuraIncreaseHealthSite>() == onMiss);
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthOutcomesAreTheSwitchs)
{
    // A bare Creature: HandleStatModifier records the amount in its modifier group and, with its stats not yet
    // modifiable, touches nothing else.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit* target = &creature;
    Aura* aura = ModeAura();
    aura->GetModifier()->m_amount = 7;
    bool apply = true;
    bool real = false;
    AuraIncreaseHealthContext ctx(aura, target, apply, real);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Not a real apply: the flat labels return from the member without touching the target, and the percentage
    // labels fall into the flat body (no break) and return with it.
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(12976, ctx).IsReturn());
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(54443, ctx).IsReturn());
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(61254, ctx).IsReturn());
    CHECK_EQ(aura->GetModifier()->m_amount, 7);
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 0.0f);

    // A miss runs the default, which falls off the switch's end: it adds the amount as a flat health bonus and
    // continues after the switch.
    SpellHandlerOutcome<void> miss = game.Dispatch<AuraIncreaseHealthSite>(60430, ctx);
    CHECK(miss.IsContinue());
    CHECK(!miss.IsReturn());
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 7.0f);
    apply = false;                                              // the remove takes it back
    CHECK(game.Dispatch<AuraIncreaseHealthSite>(60430, ctx).IsContinue());
    CHECK_EQ(creature.GetModifierValue(UNIT_MOD_HEALTH, TOTAL_VALUE), 0.0f);
    aura->GetModifier()->m_amount = 0;
}

TEST(AuraPeriodicHandlers_TheIncreaseHealthContextAliasesTheTargetApplyAndReal)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    bool apply = true;
    bool real = false;
    AuraIncreaseHealthContext ctx(ModeAura(), target, apply, real);
    CHECK(ctx.aura == ModeAura());
    CHECK(ctx.target == target);
    CHECK(ctx.apply);
    CHECK(!ctx.real);
    ctx.target = reinterpret_cast<Unit*>(units[1]);
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    apply = false;
    real = true;
    CHECK(!ctx.apply);
    CHECK(ctx.real);
}

TEST(SpellEffectHealPowerHandlers_TheEnergizeSiteHoldsItsNineLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectHealPowerHandlers(registry), uint32(10)); // nine rows and the default
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectEnergizeSite::Key), std::size_t(9));

    // Nine labels, five bodies (the four base-mana labels share one, the two injectors another), and a default that
    // is none of them.
    typedef SpellHandler<SpellEffectEnergizeSite>::Function EnergizeBody;
    EnergizeBody restoreEnergy = registry.Find<SpellEffectEnergizeSite>(9512);
    EnergizeBody bloodFury = registry.Find<SpellEffectEnergizeSite>(24571);
    EnergizeBody burstOfEnergy = registry.Find<SpellEffectEnergizeSite>(24532);
    EnergizeBody baseManaPercent = registry.Find<SpellEffectEnergizeSite>(31930);
    EnergizeBody injector = registry.Find<SpellEffectEnergizeSite>(67487);
    EnergizeBody onMiss = registry.FindDefault<SpellEffectEnergizeSite>();
    std::set<EnergizeBody> bodies = { restoreEnergy, bloodFury, burstOfEnergy, baseManaPercent, injector, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(6));
    for (uint32 spellId : { 48542u, 63375u, 68082u })
    {
        CHECK(registry.Find<SpellEffectEnergizeSite>(spellId) == baseManaPercent);
    }
    CHECK(registry.Find<SpellEffectEnergizeSite>(67490) == injector);
    CHECK(registry.Find<SpellEffectEnergizeSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and changes nothing.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 100;
    int level_diff = 0;
    int level_multiplier = 0;
    SpellEffectEnergizeContext ctx(caster, target, damage, level_diff, level_multiplier);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectEnergizeSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(damage, int32(100));
    CHECK_EQ(level_diff, 0);
    CHECK_EQ(level_multiplier, 0);
    CHECK(caster == NULL);
    CHECK(target == NULL);

    // The injectors on a target that is not a player: the type test fails, the amount stays, and the member goes on.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    target = &creature;
    for (uint32 spellId : { 67487u, 67490u })
    {
        SpellHandlerOutcome<void> injected = registry.Dispatch<SpellEffectEnergizeSite>(spellId, ctx);
        CHECK(injected.IsContinue());
        CHECK(!injected.IsReturn());
        CHECK_EQ(damage, int32(100));
    }
    target = NULL;

    // Registering again on the same table changes nothing: the keys and the default are taken.
    CHECK_EQ(RegisterSpellEffectHealPowerHandlers(registry), uint32(10));
    CHECK_EQ(registry.Count(), std::size_t(9));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectEnergizeSite>(9512) == restoreEnergy);
    CHECK(registry.FindDefault<SpellEffectEnergizeSite>() == onMiss);

    // Keyed on the energize site only: its labels are no other site's, and the other sites' are not its; the game's
    // table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 9512u, 24571u, 24532u, 31930u, 48542u, 63375u, 68082u, 67487u, 67490u })
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectEnergizeSite::Key, spellId));
        CHECK(game.Find<SpellEffectEnergizeSite>(spellId) == registry.Find<SpellEffectEnergizeSite>(spellId));
    }
    for (uint32 spellId : { 29886u, 41099u, 54833u, 29166u, 57669u, 12976u })
    {
        CHECK(game.Find<SpellEffectEnergizeSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectEnergizeSite::Key), std::size_t(9));
    CHECK(game.FindDefault<SpellEffectEnergizeSite>() == onMiss);
}

TEST(SpellEffectHealPowerHandlers_TheEnergizeContextAliasesTheSpellAndTheLevelLocals)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 100;
    int level_diff = 0;
    int level_multiplier = 0;
    SpellEffectEnergizeContext ctx(caster, target, damage, level_diff, level_multiplier);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.unitTarget == target);
    CHECK_EQ(ctx.damage, int32(100));
    CHECK_EQ(ctx.level_diff, 0);
    CHECK_EQ(ctx.level_multiplier, 0);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 125;                                           // a body's write to `damage`...
    CHECK_EQ(damage, int32(125));                               // ...is the spell's amount, which it reads after
    damage = 90;
    CHECK_EQ(ctx.damage, int32(90));
    ctx.level_diff = 7;                                         // the two locals the member shrinks the amount by
    ctx.level_multiplier = 10;
    CHECK_EQ(level_diff, 7);
    CHECK_EQ(level_multiplier, 10);
    level_diff = -5;
    level_multiplier = 4;
    CHECK_EQ(ctx.level_diff, -5);
    CHECK_EQ(ctx.level_multiplier, 4);
}

namespace
{
    // Spell.dbc rows in raw storage, as the loader makes them (SpellEntry has no default constructor: it declares a
    // private copy constructor); zeroed here, with the id set.
    struct ObjectCombatSpellRow
    {
        alignas(SpellEntry) unsigned char bytes[sizeof(SpellEntry)];
    };

    ObjectCombatSpellRow s_objectCombatRows[2] = {};

    SpellEntry const* ObjectCombatSpell(std::size_t slot, uint32 id)
    {
        SpellEntry* row = reinterpret_cast<SpellEntry*>(s_objectCombatRows[slot].bytes);
        row->ID = id;
        return row;
    }

    // The 15 Wind Stone summons, the 11 Simon Game spells, the 5 Skettis summons, Place Fake Fur and Summon Ahune
    // Lieutenant, in the switch's order.
    uint32 const ACTIVATE_OBJECT_IDS[] =
    {
        24734, 24744, 24756, 24758, 24760, 24763, 24765, 24768, 24770, 24772, 24784, 24786, 24788, 24789, 24790,
        40176, 40177, 40178, 40179, 40283, 40284, 40285, 40286, 40494, 40495, 40512,
        40632, 40640, 40642, 40644, 41004,
        46085,
        46592
    };
}

TEST(SpellEffectObjectCombatHandlers_TheActivateObjectSiteHoldsItsThirtyThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37)); // 33 + 3 rows and the resurrect default
    CHECK_EQ(registry.Count(), std::size_t(36));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellEffectActivateObjectSite::Key), std::size_t(33));

    // 33 labels, five bodies: the Wind Stone summons share one, the Simon Game spells another, the Skettis summons a
    // third; Place Fake Fur and Summon Ahune Lieutenant each have their own.
    typedef SpellHandler<SpellEffectActivateObjectSite>::Function ActivateBody;
    ActivateBody windStone = registry.Find<SpellEffectActivateObjectSite>(24734);
    ActivateBody simonGame = registry.Find<SpellEffectActivateObjectSite>(40176);
    ActivateBody skettis = registry.Find<SpellEffectActivateObjectSite>(40632);
    ActivateBody fakeFur = registry.Find<SpellEffectActivateObjectSite>(46085);
    ActivateBody ahune = registry.Find<SpellEffectActivateObjectSite>(46592);
    std::set<ActivateBody> bodies = { windStone, simonGame, skettis, fakeFur, ahune };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));
    for (std::size_t i = 0; i < 33; ++i)
    {
        ActivateBody body = registry.Find<SpellEffectActivateObjectSite>(ACTIVATE_OBJECT_IDS[i]);
        CHECK(body == (i < 15 ? windStone : i < 26 ? simonGame : i < 31 ? skettis : i == 31 ? fakeFur : ahune));
    }

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the misc value's case goes on to
    // its own `break;` with nothing changed.
    CHECK(registry.FindDefault<SpellEffectActivateObjectSite>() == NULL);
    CHECK(registry.Find<SpellEffectActivateObjectSite>(12345) == NULL);
    GameObject* target = NULL;
    Unit* caster = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellEffectActivateObjectContext ctx(target, caster, spellInfo);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectActivateObjectSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(caster == NULL);
    CHECK(spellInfo->ID == 12345);

    // Registering again on the same table changes nothing: every key and the resurrect default are taken.
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.Count(), std::size_t(36));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellEffectActivateObjectSite>(24734) == windStone);
    CHECK(registry.Find<SpellEffectActivateObjectSite>(46592) == ahune);

    // Keyed on the activate-object site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ACTIVATE_OBJECT_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectActivateObjectSite::Key, spellId));
        CHECK(game.Find<SpellEffectActivateObjectSite>(spellId) == registry.Find<SpellEffectActivateObjectSite>(spellId));
    }
    for (uint32 spellId : { 8342u, 22999u, 54732u, 29886u, 9512u, 41099u, 54833u })
    {
        CHECK(game.Find<SpellEffectActivateObjectSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectActivateObjectSite::Key), std::size_t(33));
    CHECK(game.FindDefault<SpellEffectActivateObjectSite>() == NULL);
}

TEST(SpellEffectObjectCombatHandlers_TheActivateObjectContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    GameObject* target = reinterpret_cast<GameObject*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 24734);
    SpellEffectActivateObjectContext ctx(target, caster, spellInfo);
    CHECK(ctx.gameObjTarget == target);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_spellInfo == spellInfo);

    ctx.gameObjTarget = reinterpret_cast<GameObject*>(objects[2]); // the target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<GameObject*>(objects[2]));
    target = reinterpret_cast<GameObject*>(objects[0]);
    CHECK(ctx.gameObjTarget == reinterpret_cast<GameObject*>(objects[0]));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // so is the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 40176);              // and the spell's entry
    CHECK(spellInfo->ID == 40176);
    spellInfo = ObjectCombatSpell(0, 24734);
    CHECK(ctx.m_spellInfo->ID == 24734);
}

TEST(SpellEffectObjectCombatHandlers_TheResurrectSiteHoldsItsThreeLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));

    // Three labels, one body, and a default that is not it.
    typedef SpellHandler<SpellEffectResurrectSite>::Function ResurrectBody;
    ResurrectBody defibrillate = registry.Find<SpellEffectResurrectSite>(8342);
    ResurrectBody onMiss = registry.FindDefault<SpellEffectResurrectSite>();
    CHECK(defibrillate != NULL);
    CHECK(onMiss != NULL);
    CHECK(defibrillate != onMiss);
    CHECK(registry.Find<SpellEffectResurrectSite>(22999) == defibrillate);
    CHECK(registry.Find<SpellEffectResurrectSite>(54732) == defibrillate);
    CHECK(registry.Find<SpellEffectResurrectSite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and changes nothing.
    Unit* caster = NULL;
    Item* castItem = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellEffectResurrectContext ctx(caster, castItem, spellInfo);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellEffectResurrectSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK(caster == NULL);
    CHECK(castItem == NULL);

    // 54732 has no failure spell, so its body needs no live caster: a failed roll (33 in 100) answers Return, ending
    // the resurrection, and a successful one Continue; never a miss, and neither casts. 400 rolls see both.
    spellInfo = ObjectCombatSpell(0, 54732);
    uint32 returned = 0;
    uint32 continued = 0;
    for (uint32 roll = 0; roll < 400; ++roll)
    {
        SpellHandlerOutcome<void> rolled = registry.Dispatch<SpellEffectResurrectSite>(54732, ctx);
        CHECK(rolled.IsReturn() || rolled.IsContinue());
        if (rolled.IsReturn())
        {
            ++returned;
        }
        else if (rolled.IsContinue())
        {
            ++continued;
        }
    }
    CHECK_EQ(returned + continued, uint32(400));
    CHECK(returned > 0);
    CHECK(continued > 0);
    CHECK(caster == NULL);
    CHECK(castItem == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectObjectCombatHandlers(registry), uint32(37));
    CHECK_EQ(registry.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));
    CHECK(registry.Find<SpellEffectResurrectSite>(8342) == defibrillate);
    CHECK(registry.FindDefault<SpellEffectResurrectSite>() == onMiss);

    // Keyed on the resurrect site only; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 8342u, 22999u, 54732u })
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectResurrectSite::Key, spellId));
        CHECK(game.Find<SpellEffectResurrectSite>(spellId) == defibrillate);
    }
    for (uint32 spellId : { 24734u, 40176u, 46592u, 29886u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellEffectResurrectSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectResurrectSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectResurrectSite>() == onMiss);
}

TEST(SpellEffectObjectCombatHandlers_TheResurrectContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    Unit* caster = reinterpret_cast<Unit*>(objects[0]);
    Item* castItem = reinterpret_cast<Item*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 8342);
    SpellEffectResurrectContext ctx(caster, castItem, spellInfo);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_CastItem == castItem);
    CHECK(ctx.m_spellInfo == spellInfo);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[2]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[2]));
    caster = reinterpret_cast<Unit*>(objects[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[0]));
    ctx.m_CastItem = reinterpret_cast<Item*>(objects[3]);       // so is the item it is cast with
    CHECK(castItem == reinterpret_cast<Item*>(objects[3]));
    castItem = NULL;
    CHECK(ctx.m_CastItem == NULL);
    ctx.m_spellInfo = ObjectCombatSpell(1, 22999);              // and the spell's entry
    CHECK(spellInfo->ID == 22999);
    spellInfo = ObjectCombatSpell(0, 8342);
    CHECK(ctx.m_spellInfo->ID == 8342);
}

namespace
{
    // The nine labels that keep the caster's victim out of the targets, then Bloodboil, in the switch's order.
    uint32 const ALL_ENEMY_IN_AREA_IDS[] = { 30769, 30843, 31347, 37676, 38028, 40618, 41376, 62166, 63981, 42005 };

    // Places the creature in map 0's world frame at (x, 0, 0).
    void PlaceTargetAt(Creature& creature, float x)
    {
        creature.Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(x, 0.0f, 0.0f), 0.0f);
    }

    /// A Creature with its update fields allocated (a cast's unit target reads its guid), never in a world.
    class ValuedCreature : public Creature
    {
        public:
            ValuedCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ValuedCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(SpellTargetingHandlers_TheAllEnemyInAreaSiteHoldsItsTenLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13)); // 10 + 1 rows and the two defaults
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK_EQ(registry.CountAt(SpellTargetAllEnemyInAreaSite::Key), std::size_t(10));

    // Ten labels, two bodies: the nine victim labels share one, Bloodboil has its own; the default is neither.
    typedef SpellHandler<SpellTargetAllEnemyInAreaSite>::Function AreaBody;
    AreaBody skipVictim = registry.Find<SpellTargetAllEnemyInAreaSite>(30769);
    AreaBody bloodboil = registry.Find<SpellTargetAllEnemyInAreaSite>(42005);
    AreaBody onMiss = registry.FindDefault<SpellTargetAllEnemyInAreaSite>();
    std::set<AreaBody> bodies = { skipVictim, bloodboil, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    for (std::size_t i = 0; i < 10; ++i)
    {
        AreaBody body = registry.Find<SpellTargetAllEnemyInAreaSite>(ALL_ENEMY_IN_AREA_IDS[i]);
        CHECK(body == (i < 9 ? skipVictim : bloodboil));
    }
    CHECK(registry.Find<SpellTargetAllEnemyInAreaSite>(12345) == NULL);

    // A caster at the origin and two targets in the area, 5 and 10 yards from it; the effect may hit one target.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    Creature near(CREATURE_SUBTYPE_GENERIC);
    Creature far(CREATURE_SUBTYPE_GENERIC);
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(near, 5.0f);
    PlaceTargetAt(far, 10.0f);
    Unit* casterMember = &caster;
    std::list<Unit*> targets = { &near, &far };
    uint32 maxTargets = 1;
    SpellTargetAllEnemyInAreaContext ctx(casterMember, targets, maxTargets);

    // A miss runs the default, which answers Continue as a switch's `default: break;` does and keeps the targets.
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK_EQ(targets.size(), std::size_t(2));
    CHECK(targets.front() == &near);
    CHECK(targets.back() == &far);

    // The victim labels on a caster with no victim: nothing to drop, and the member goes on.
    for (std::size_t i = 0; i < 9; ++i)
    {
        uint32 spellId = ALL_ENEMY_IN_AREA_IDS[i];
        SpellHandlerOutcome<void> skipped = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(spellId, ctx);
        CHECK(skipped.IsContinue());
        CHECK(!skipped.IsReturn());
        CHECK_EQ(targets.size(), std::size_t(2));
        CHECK(targets.front() == &near);
    }

    // Bloodboil reads the most targets the effect may hit as the member holds it when the body runs: as many targets
    // as that are kept as they are; more are sorted furthest from the caster first and cut to that many.
    maxTargets = 2;
    SpellHandlerOutcome<void> kept = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(42005, ctx);
    CHECK(kept.IsContinue());
    CHECK(!kept.IsReturn());
    CHECK_EQ(targets.size(), std::size_t(2));
    CHECK(targets.front() == &near);
    CHECK(targets.back() == &far);
    maxTargets = 1;
    SpellHandlerOutcome<void> cut = registry.Dispatch<SpellTargetAllEnemyInAreaSite>(42005, ctx);
    CHECK(cut.IsContinue());
    CHECK(!cut.IsReturn());
    REQUIRE(targets.size() == 1);
    CHECK(targets.front() == &far);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: every key and both defaults are taken.
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.Count(), std::size_t(11));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK(registry.Find<SpellTargetAllEnemyInAreaSite>(30769) == skipVictim);
    CHECK(registry.FindDefault<SpellTargetAllEnemyInAreaSite>() == onMiss);

    // Keyed on the area site only: its labels are no other site's, and the other sites' are not its; the game's table
    // holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : ALL_ENEMY_IN_AREA_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellTargetAllEnemyInAreaSite::Key, spellId));
        AreaBody body = registry.Find<SpellTargetAllEnemyInAreaSite>(spellId);
        CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(spellId) == body);
    }
    for (uint32 spellId : { 20577u, 24734u, 8342u, 9512u, 29886u, 41099u, 54833u })
    {
        CHECK(game.Find<SpellTargetAllEnemyInAreaSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellTargetAllEnemyInAreaSite::Key), std::size_t(10));
    CHECK(game.FindDefault<SpellTargetAllEnemyInAreaSite>() == onMiss);
}

TEST(SpellTargetingHandlers_TheAllEnemyInAreaContextAliasesTheSpellAndTheLocals)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    std::list<Unit*> targets;
    uint32 maxTargets = 5;
    SpellTargetAllEnemyInAreaContext ctx(caster, targets, maxTargets);
    CHECK(ctx.m_caster == caster);
    CHECK(&ctx.targetUnitMap == &targets);
    CHECK_EQ(ctx.unMaxTargets, uint32(5));

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.targetUnitMap.push_back(caster);                        // the target list is the member's parameter
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.targetUnitMap.empty());
    maxTargets = 3;                                             // and the most targets is the member's local, read
    CHECK_EQ(ctx.unMaxTargets, uint32(3));
}

TEST(SpellTargetingHandlers_TheEffectDummySiteHoldsCannibalizeAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));

    // One label, and a default that is not its body.
    typedef SpellHandler<SpellTargetEffectDummySite>::Function DummyBody;
    DummyBody cannibalize = registry.Find<SpellTargetEffectDummySite>(20577);
    DummyBody onMiss = registry.FindDefault<SpellTargetEffectDummySite>();
    CHECK(cannibalize != NULL);
    CHECK(onMiss != NULL);
    CHECK(cannibalize != onMiss);
    CHECK(registry.Find<SpellTargetEffectDummySite>(12345) == NULL);

    // A miss runs the default, which answers Continue as a switch's `default:` does and targets the unit target the
    // cast holds, if any; it reads nothing else of the spell.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    ValuedCreature target;
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(target, 5.0f);
    Spell* spell = NULL;
    Unit* casterMember = &caster;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 12345);
    SpellCastTargets castTargets;
    std::list<Unit*> targets;
    SpellTargetEffectDummyContext ctx(spell, casterMember, spellInfo, castTargets, targets);
    SpellHandlerOutcome<void> missed = registry.Dispatch<SpellTargetEffectDummySite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());
    CHECK(targets.empty());
    castTargets.setUnitTarget(&target);
    SpellHandlerOutcome<void> targeted = registry.Dispatch<SpellTargetEffectDummySite>(12345, ctx);
    CHECK(targeted.IsContinue());
    CHECK(!targeted.IsReturn());
    REQUIRE(targets.size() == 1);
    CHECK(targets.front() == &target);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellTargetingHandlers(registry), uint32(13));
    CHECK_EQ(registry.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));
    CHECK(registry.Find<SpellTargetEffectDummySite>(20577) == cannibalize);
    CHECK(registry.FindDefault<SpellTargetEffectDummySite>() == onMiss);

    // Keyed on the dummy-effect site only; the game's table holds the same row and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(!HeldByAnotherSite(game, SpellTargetEffectDummySite::Key, 20577));
    CHECK(game.Find<SpellTargetEffectDummySite>(20577) == cannibalize);
    for (uint32 spellId : { 30769u, 42005u, 24734u, 8342u, 9512u, 29886u, 41099u })
    {
        CHECK(game.Find<SpellTargetEffectDummySite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellTargetEffectDummySite::Key), std::size_t(1));
    CHECK(game.FindDefault<SpellTargetEffectDummySite>() == onMiss);
}

TEST(SpellTargetingHandlers_TheEffectDummyContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[3][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 20577);
    SpellCastTargets castTargets;
    std::list<Unit*> targets;
    SpellTargetEffectDummyContext ctx(spell, caster, spellInfo, castTargets, targets);
    CHECK(ctx.spell == spell);
    CHECK(ctx.m_caster == caster);
    CHECK(ctx.m_spellInfo == spellInfo);
    CHECK(&ctx.m_targets == &castTargets);
    CHECK(&ctx.targetUnitMap == &targets);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[2]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[2]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 12345);              // so is the spell's entry
    CHECK(spellInfo->ID == 12345);
    spellInfo = ObjectCombatSpell(0, 20577);
    CHECK(ctx.m_spellInfo->ID == 20577);
    ctx.m_targets.m_targetMask = TARGET_FLAG_DEST_LOCATION;     // and its targets, which the member reads after
    CHECK_EQ(castTargets.m_targetMask, uint32(TARGET_FLAG_DEST_LOCATION));
    castTargets.m_targetMask = 0;
    CHECK_EQ(ctx.m_targets.m_targetMask, uint32(0));
    ctx.targetUnitMap.push_back(caster);                        // the target list is the member's parameter
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.targetUnitMap.empty());
}

TEST(SpellChecksHandlers_TheCastAuraDummySiteHoldsItsTwoLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellChecksHandlers(registry), uint32(3)); // 2 rows and the default
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellCheckCastAuraDummySite::Key), std::size_t(2));

    // Two labels, two bodies; the default is neither.
    typedef SpellHandler<SpellCheckCastAuraDummySite>::Function CastBody;
    CastBody killCommand = registry.Find<SpellCheckCastAuraDummySite>(34026);
    CastBody survivalInstincts = registry.Find<SpellCheckCastAuraDummySite>(61336);
    CastBody onMiss = registry.FindDefault<SpellCheckCastAuraDummySite>();
    std::set<CastBody> bodies = { killCommand, survivalInstincts, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    CHECK(registry.Find<SpellCheckCastAuraDummySite>(12345) == NULL);

    // A creature caster with no pet, in no world.
    ValuedCreature caster;
    Unit* casterMember = &caster;
    SpellCheckCastAuraDummyContext ctx(casterMember);

    // A miss runs the default, which answers Continue as the switch's `default: break;` does.
    SpellHandlerOutcome<SpellCastResult> missed = registry.Dispatch<SpellCheckCastAuraDummySite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());

    // Kill Command with no pet fails the cast with its result.
    SpellHandlerOutcome<SpellCastResult> noPet = registry.Dispatch<SpellCheckCastAuraDummySite>(34026, ctx);
    REQUIRE(noPet.IsReturn());
    CHECK(noPet.GetValue() == SPELL_FAILED_NO_PET);

    // Survival Instincts on a caster that is not a player fails the cast with its result.
    SpellHandlerOutcome<SpellCastResult> notFeral = registry.Dispatch<SpellCheckCastAuraDummySite>(61336, ctx);
    REQUIRE(notFeral.IsReturn());
    CHECK(notFeral.GetValue() == SPELL_FAILED_ONLY_SHAPESHIFT);
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: both keys and the default are taken.
    CHECK_EQ(RegisterSpellChecksHandlers(registry), uint32(3));
    CHECK_EQ(registry.Count(), std::size_t(2));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellCheckCastAuraDummySite>(34026) == killCommand);
    CHECK(registry.FindDefault<SpellCheckCastAuraDummySite>() == onMiss);

    // Keyed on the cast-check site only; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 34026u, 61336u })
    {
        CHECK(!HeldByAnotherSite(game, SpellCheckCastAuraDummySite::Key, spellId));
        CHECK(game.Find<SpellCheckCastAuraDummySite>(spellId) == registry.Find<SpellCheckCastAuraDummySite>(spellId));
    }
    for (uint32 spellId : { 37433u, 68921u, 69049u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellCheckCastAuraDummySite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellCheckCastAuraDummySite::Key), std::size_t(2));
    CHECK(game.FindDefault<SpellCheckCastAuraDummySite>() == onMiss);
}

TEST(SpellChecksHandlers_TheCastAuraDummyContextAliasesTheCaster)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    SpellCheckCastAuraDummyContext ctx(caster);
    CHECK(ctx.m_caster == caster);

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

TEST(SpellCheckTargetHandlers_TheTargetSiteHoldsItsThreeLabelsAndTheDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellCheckTargetHandlers(registry), uint32(4)); // 3 rows and the default
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK_EQ(registry.CountAt(SpellCheckTargetSite::Key), std::size_t(3));

    // Three labels, two bodies: the two Soulstorm labels share one, Spout has its own; the default is neither.
    typedef SpellHandler<SpellCheckTargetSite>::Function TargetBody;
    TargetBody spout = registry.Find<SpellCheckTargetSite>(37433);
    TargetBody soulstorm = registry.Find<SpellCheckTargetSite>(68921);
    TargetBody onMiss = registry.FindDefault<SpellCheckTargetSite>();
    std::set<TargetBody> bodies = { spout, soulstorm, onMiss };
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));
    CHECK(registry.Find<SpellCheckTargetSite>(69049) == soulstorm);
    CHECK(registry.Find<SpellCheckTargetSite>(12345) == NULL);

    // A caster at the origin and two creature targets, 5 and 15 yards from it.
    Creature caster(CREATURE_SUBTYPE_GENERIC);
    Creature near(CREATURE_SUBTYPE_GENERIC);
    Creature far(CREATURE_SUBTYPE_GENERIC);
    PlaceTargetAt(caster, 0.0f);
    PlaceTargetAt(near, 5.0f);
    PlaceTargetAt(far, 15.0f);
    Unit* casterMember = &caster;
    Unit* target = &near;
    SpellCheckTargetContext ctx(casterMember, target);

    // A miss runs the default, which answers Continue as the switch's `default: break;` does: the target stands.
    SpellHandlerOutcome<bool> missed = registry.Dispatch<SpellCheckTargetSite>(12345, ctx);
    CHECK(missed.IsContinue());
    CHECK(!missed.IsReturn());
    CHECK(!missed.IsMiss());

    // Spout refuses a target that is not a player.
    SpellHandlerOutcome<bool> notPlayer = registry.Dispatch<SpellCheckTargetSite>(37433, ctx);
    REQUIRE(notPlayer.IsReturn());
    CHECK(!notPlayer.GetValue());

    // Soulstorm refuses a target within 10 yards of the caster and lets one further away stand; the body reads the
    // target the member holds when it runs.
    for (uint32 spellId : { 68921u, 69049u })
    {
        target = &near;
        SpellHandlerOutcome<bool> tooNear = registry.Dispatch<SpellCheckTargetSite>(spellId, ctx);
        REQUIRE(tooNear.IsReturn());
        CHECK(!tooNear.GetValue());
        target = &far;
        SpellHandlerOutcome<bool> farEnough = registry.Dispatch<SpellCheckTargetSite>(spellId, ctx);
        CHECK(farEnough.IsContinue());
        CHECK(!farEnough.IsReturn());
    }
    CHECK(casterMember == &caster);

    // Registering again on the same table changes nothing: every key and the default are taken.
    CHECK_EQ(RegisterSpellCheckTargetHandlers(registry), uint32(4));
    CHECK_EQ(registry.Count(), std::size_t(3));
    CHECK_EQ(registry.CountDefaults(), std::size_t(1));
    CHECK(registry.Find<SpellCheckTargetSite>(37433) == spout);
    CHECK(registry.FindDefault<SpellCheckTargetSite>() == onMiss);

    // Keyed on the target-check site only; the game's table holds the same rows and the same default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : { 37433u, 68921u, 69049u })
    {
        CHECK(!HeldByAnotherSite(game, SpellCheckTargetSite::Key, spellId));
        CHECK(game.Find<SpellCheckTargetSite>(spellId) == registry.Find<SpellCheckTargetSite>(spellId));
    }
    for (uint32 spellId : { 34026u, 61336u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellCheckTargetSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellCheckTargetSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellCheckTargetSite>() == onMiss);
}

TEST(SpellCheckTargetHandlers_TheTargetContextAliasesTheCasterAndTheTarget)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellCheckTargetContext ctx(caster, target);
    CHECK(ctx.m_caster == caster);
    CHECK(&ctx.target == &target);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    target = reinterpret_cast<Unit*>(units[3]);                 // and the target is the member's parameter, read
    CHECK(ctx.target == reinterpret_cast<Unit*>(units[3]));
}

namespace
{
    // SpellEffect.dbc rows in raw storage, as the loader makes them; zeroed here, with the effect index set.
    struct WeaponDamageEffectRow
    {
        alignas(SpellEffectEntry) unsigned char bytes[sizeof(SpellEffectEntry)];
    };

    WeaponDamageEffectRow s_weaponDamageEffects[2] = {};

    SpellEffectEntry const* WeaponDamageEffect(std::size_t slot, uint32 effectIndex)
    {
        SpellEffectEntry* row = reinterpret_cast<SpellEffectEntry*>(s_weaponDamageEffects[slot].bytes);
        row->EffectIndex = effectIndex;
        return row;
    }

    uint32 const WEAPON_DAMAGE_IDS[] = { 66765, 66809, 67331, 67333, 69055, 71021 };

    // Three targets whose effect masks are 1, 1 and 2: two carry effect 0, one carries effect 1.
    Spell::TargetList WeaponDamageTargets()
    {
        Spell::TargetList targets;
        for (uint8 mask : { uint8(1), uint8(1), uint8(2) })
        {
            Spell::TargetList::value_type entry = Spell::TargetList::value_type();
            entry.effectMask = mask;
            targets.push_back(entry);
        }
        return targets;
    }
}

TEST(SpellEffectSkillEnchantPetHandlers_TheWeaponDamageSiteHoldsItsSixLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectSkillEnchantPetHandlers(registry), uint32(6)); // 6 rows, no default
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectWeaponDmgSite::Key), std::size_t(6));

    // Six labels, one body.
    typedef SpellHandler<SpellEffectWeaponDmgSite>::Function WeaponDamageBody;
    WeaponDamageBody divide = registry.Find<SpellEffectWeaponDmgSite>(66765);
    CHECK(divide != NULL);
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        CHECK(registry.Find<SpellEffectWeaponDmgSite>(spellId) == divide);
    }

    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    float totalDamagePercentMod = 1.0f;
    SpellEffectWeaponDmgContext ctx(targets, effect, totalDamagePercentMod);

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family's case goes on to its
    // own `break;` with nothing changed.
    CHECK(registry.FindDefault<SpellEffectWeaponDmgSite>() == NULL);
    CHECK(registry.Find<SpellEffectWeaponDmgSite>(12345) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectWeaponDmgSite>(12345, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(totalDamagePercentMod == 1.0f);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK_EQ(effect->EffectIndex, uint32(0));

    // Each label divides the multiplier by the number of targets the effect hits and continues: effect 0 hits two
    // of the three targets, effect 1 one. The body reads the effect the member holds when it runs.
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        effect = WeaponDamageEffect(0, 0);
        totalDamagePercentMod = 1.0f;
        SpellHandlerOutcome<void> first = registry.Dispatch<SpellEffectWeaponDmgSite>(spellId, ctx);
        CHECK(first.IsContinue());
        CHECK(!first.IsReturn());
        CHECK(totalDamagePercentMod == 0.5f);

        effect = WeaponDamageEffect(1, 1);
        totalDamagePercentMod = 1.0f;
        SpellHandlerOutcome<void> second = registry.Dispatch<SpellEffectWeaponDmgSite>(spellId, ctx);
        CHECK(second.IsContinue());
        CHECK(!second.IsReturn());
        CHECK(totalDamagePercentMod == 1.0f);
    }
    CHECK_EQ(targets.size(), std::size_t(3));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectSkillEnchantPetHandlers(registry), uint32(6));
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<SpellEffectWeaponDmgSite>(71021) == divide);

    // Keyed on the weapon-damage site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : WEAPON_DAMAGE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectWeaponDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectWeaponDmgSite>(spellId) == registry.Find<SpellEffectWeaponDmgSite>(spellId));
    }
    for (uint32 spellId : { 34026u, 61336u, 37433u, 68921u, 69049u, 20577u, 30769u, 24734u, 8342u, 9512u, 41099u })
    {
        CHECK(game.Find<SpellEffectWeaponDmgSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectWeaponDmgSite::Key), std::size_t(6));
    CHECK(game.FindDefault<SpellEffectWeaponDmgSite>() == NULL);
}

TEST(SpellEffectSkillEnchantPetHandlers_TheWeaponDamageContextAliasesTheSpell)
{
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    float totalDamagePercentMod = 1.0f;
    SpellEffectWeaponDmgContext ctx(targets, effect, totalDamagePercentMod);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);
    CHECK(&ctx.totalDamagePercentMod == &totalDamagePercentMod);

    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // the target list is the spell's member
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    ctx.totalDamagePercentMod = 0.25f;                                  // the multiplier is the member's local
    CHECK(totalDamagePercentMod == 0.25f);
    totalDamagePercentMod = 1.0f;
    CHECK(ctx.totalDamagePercentMod == 1.0f);
    effect = WeaponDamageEffect(1, 1);                                  // and the effect is its parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

namespace
{
    // The sixty ids whose damage is divided among the targets, in the switch's order.
    uint32 const SCHOOL_DAMAGE_DIVIDE_IDS[] =
    {
        24340, 26558, 28884, 36837, 38903, 41276, 57467, 26789, 31436, 35181, 40810, 43267, 43268, 42384, 45150,
        64422, 64688, 70492, 72505, 71904, 72624, 72625, 77679, 92968, 92969, 92970, 82935, 88915, 88916, 88917,
        86014, 92863, 92864, 92865, 86367, 93135, 93136, 93137, 86825, 92879, 92880, 92881, 88942, 95172, 89348,
        95178, 98474, 100212, 100213, 100214, 103414, 108571, 109033, 109034, 105069, 108094, 106375, 109182, 109183,
        109184
    };

    // The fourteen other school-damage labels, in the switch's order.
    uint32 const SCHOOL_DAMAGE_OTHER_IDS[] =
    {
        25599, 20253, 61491, 29142, 35139, 49882, 55269, 37841, 38441, 50341, 62775, 67485, 68793, 69050
    };

    // The trigger-spell, recall and post-teleport labels, in the switches' order.
    uint32 const TRIGGER_SPELL_IDS[] = { 18461, 29284, 29286, 31980, 35729, 41967, 58832 };
    uint32 const TELEPORT_RECALL_IDS[] = { 48129, 60320, 60321 };
    uint32 const TELEPORT_POST_IDS[] = { 23442, 36941, 36890 };

    // The file registers 74 + 7 + 3 + 3 rows and no default.
    uint32 const DAMAGE_TELEPORT_ROWS = 87;
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageSiteHoldsItsSeventyFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectSchoolDmgSite::Key), std::size_t(74));

    // 74 labels, eleven bodies: the sixty divided-damage ids share one, Intercept's two another, the four
    // percent-of-health ids a third; every other label has its own.
    typedef SpellHandler<SpellEffectSchoolDmgSite>::Function SchoolDamageBody;
    SchoolDamageBody divide = registry.Find<SpellEffectSchoolDmgSite>(24340);
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        CHECK(registry.Find<SpellEffectSchoolDmgSite>(spellId) == divide);
    }
    SchoolDamageBody intercept = registry.Find<SpellEffectSchoolDmgSite>(20253);
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(61491) == intercept);
    SchoolDamageBody percent = registry.Find<SpellEffectSchoolDmgSite>(29142);
    for (uint32 spellId : { 35139u, 49882u, 55269u })
    {
        CHECK(registry.Find<SpellEffectSchoolDmgSite>(spellId) == percent);
    }
    std::set<SchoolDamageBody> bodies = { divide, intercept, percent };
    for (uint32 spellId : { 25599u, 37841u, 38441u, 50341u, 62775u, 67485u, 68793u, 69050u })
    {
        bodies.insert(registry.Find<SpellEffectSchoolDmgSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(11));

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the family's case goes on to its
    // own `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    CHECK(registry.FindDefault<SpellEffectSchoolDmgSite>() == NULL);
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(40739) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectSchoolDmgSite>(40739, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK_EQ(damage, int32(90));
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK_EQ(effect->EffectIndex, uint32(0));

    // Each divided-damage label divides the damage by the number of targets the effect hits and continues: effect 0
    // hits two of the three targets, effect 1 one. The body reads the effect the member holds when it runs.
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        effect = WeaponDamageEffect(0, 0);
        damage = 90;
        SpellHandlerOutcome<void> first = registry.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx);
        CHECK(first.IsContinue());
        CHECK(!first.IsReturn());
        CHECK_EQ(damage, int32(45));

        effect = WeaponDamageEffect(1, 1);
        damage = 90;
        SpellHandlerOutcome<void> second = registry.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx);
        CHECK(second.IsContinue());
        CHECK(!second.IsReturn());
        CHECK_EQ(damage, int32(90));
    }
    CHECK_EQ(targets.size(), std::size_t(3));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK(registry.Find<SpellEffectSchoolDmgSite>(109184) == divide);

    // Keyed on the school-damage site only: its labels are no other site's, and the other sites' are not its; the
    // game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : SCHOOL_DAMAGE_DIVIDE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectSchoolDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == divide);
    }
    for (uint32 spellId : SCHOOL_DAMAGE_OTHER_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectSchoolDmgSite::Key, spellId));
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == registry.Find<SpellEffectSchoolDmgSite>(spellId));
    }
    for (uint32 spellId : { 18461u, 58832u, 48129u, 23442u, 66765u, 34026u, 37433u, 8342u, 41099u })
    {
        CHECK(game.Find<SpellEffectSchoolDmgSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectSchoolDmgSite::Key), std::size_t(74));
    CHECK(game.FindDefault<SpellEffectSchoolDmgSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageBodiesSetTheDamageFromTheUnits)
{
    // Creatures with their update fields allocated: the caster's attack power 1000, the target's health and maximum
    // health 1000, no mana; neither is a player.
    ValuedCreature casterUnit;
    ValuedCreature targetUnit;
    casterUnit.SetInt32Value(UNIT_FIELD_ATTACK_POWER, 1000);
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 1000);
    targetUnit.SetUInt32Value(UNIT_FIELD_MAXHEALTH, 1000);
    Unit* caster = &casterUnit;
    Unit* target = &targetUnit;
    int32 damage = 0;
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();

    // Thundercrash: half the target's health, at least 200.
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(25599, ctx).IsContinue());
    CHECK_EQ(damage, int32(500));
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 300);
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(25599, ctx).IsContinue());
    CHECK_EQ(damage, int32(200));
    targetUnit.SetUInt32Value(UNIT_FIELD_HEALTH, 1000);

    // Intercept: the damage gains 12% of the caster's attack power.
    for (uint32 spellId : { 20253u, 61491u })
    {
        damage = 100;
        CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx).IsContinue());
        CHECK_EQ(damage, int32(220));
    }

    // The four percent-of-health labels: the damage is that percent of the target's maximum health.
    for (uint32 spellId : { 29142u, 35139u, 49882u, 55269u })
    {
        damage = 25;
        CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(spellId, ctx).IsContinue());
        CHECK_EQ(damage, int32(250));
    }

    // Lightning Strike: a target that is not a player is given no credit, and the damage stands.
    damage = 77;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(37841, ctx).IsContinue());
    CHECK_EQ(damage, int32(77));

    // Cataclysmic Bolt and Tympanic Tantrum: a half and a tenth of the target's maximum health.
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(38441, ctx).IsContinue());
    CHECK_EQ(damage, int32(500));
    damage = 7;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(62775, ctx).IsContinue());
    CHECK_EQ(damage, int32(100));

    // Touch the Nightmare: the third effect's damage is 30% of the target's maximum health; the others' stand.
    effect = WeaponDamageEffect(0, 2);
    damage = 9;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(50341, ctx).IsContinue());
    CHECK_EQ(damage, int32(300));
    effect = WeaponDamageEffect(0, 0);
    damage = 9;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(50341, ctx).IsContinue());
    CHECK_EQ(damage, int32(9));

    // Hand of Reckoning: the damage gains half the caster's attack power.
    damage = 100;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(67485, ctx).IsContinue());
    CHECK_EQ(damage, int32(600));

    // Magic's Bane: the target has no mana to add, so the damage stands up to the cap, 10000 and 15000 heroic.
    damage = 4000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(68793, ctx).IsContinue());
    CHECK_EQ(damage, int32(4000));
    damage = 20000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(68793, ctx).IsContinue());
    CHECK_EQ(damage, int32(10000));
    damage = 12000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(69050, ctx).IsContinue());
    CHECK_EQ(damage, int32(12000));
    damage = 20000;
    CHECK(game.Dispatch<SpellEffectSchoolDmgSite>(69050, ctx).IsContinue());
    CHECK_EQ(damage, int32(15000));

    // No body changes the units the context holds.
    CHECK(caster == &casterUnit);
    CHECK(target == &targetUnit);
}

TEST(SpellEffectDamageTeleportHandlers_TheSchoolDamageContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 10;
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectSchoolDmgContext ctx(caster, target, damage, targets, effect);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 25;                                            // and the damage
    CHECK_EQ(damage, int32(25));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // and the target list
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectDamageTeleportHandlers_TheTriggerSpellSiteHoldsItsSevenLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));

    // Seven labels, seven bodies.
    typedef SpellHandler<SpellEffectTriggerSpellSite>::Function TriggerBody;
    std::set<TriggerBody> bodies;
    for (uint32 spellId : TRIGGER_SPELL_IDS)
    {
        bodies.insert(registry.Find<SpellEffectTriggerSpellSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(7));

    // No default: a trigger id with no row (Web's 11920) finds nothing, and its dispatch is a miss, so the triggered
    // spell is looked up and cast as usual, with nothing changed.
    ValuedCreature targetUnit;
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    Unit* target = NULL;
    Unit* caster = NULL;
    Item* castItem = NULL;
    ObjectGuid originalCaster;
    SpellEffectTriggerSpellContext ctx(target, caster, castItem, originalCaster);
    CHECK(registry.FindDefault<SpellEffectTriggerSpellSite>() == NULL);
    CHECK(registry.Find<SpellEffectTriggerSpellSite>(11920) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTriggerSpellSite>(11920, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(caster == NULL);
    CHECK(castItem == NULL);
    CHECK(originalCaster.IsEmpty());

    // The bodies that need no live unit: Vanish ends the effect for a target that is not a player; Cloak of Shadows
    // with no aura on the target and Shadowfiend with no pet end it too; Mirror Image without the glyph goes on to
    // the triggered spell. The other three cast a spell and need a live unit.
    target = &targetUnit;
    caster = &casterUnit;
    SpellHandlerOutcome<void> vanish = registry.Dispatch<SpellEffectTriggerSpellSite>(18461, ctx);
    CHECK(vanish.IsReturn());
    CHECK(!vanish.IsContinue());
    SpellHandlerOutcome<void> cloak = registry.Dispatch<SpellEffectTriggerSpellSite>(35729, ctx);
    CHECK(cloak.IsReturn());
    SpellHandlerOutcome<void> shadowfiend = registry.Dispatch<SpellEffectTriggerSpellSite>(41967, ctx);
    CHECK(shadowfiend.IsReturn());
    SpellHandlerOutcome<void> mirrorImage = registry.Dispatch<SpellEffectTriggerSpellSite>(58832, ctx);
    CHECK(mirrorImage.IsContinue());
    CHECK(!mirrorImage.IsReturn());
    CHECK(target == &targetUnit);
    CHECK(caster == &casterUnit);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DAMAGE_TELEPORT_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));

    // Keyed on the trigger-spell site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TRIGGER_SPELL_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTriggerSpellSite::Key, spellId));
        CHECK(game.Find<SpellEffectTriggerSpellSite>(spellId) == registry.Find<SpellEffectTriggerSpellSite>(spellId));
    }
    for (uint32 spellId : { 745u, 24340u, 25599u, 48129u, 23442u, 66765u, 34026u, 8342u, 41099u })
    {
        CHECK(game.Find<SpellEffectTriggerSpellSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTriggerSpellSite::Key), std::size_t(7));
    CHECK(game.FindDefault<SpellEffectTriggerSpellSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTriggerSpellContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[4][16];
    Unit* target = reinterpret_cast<Unit*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Item* castItem = reinterpret_cast<Item*>(objects[2]);
    ObjectGuid originalCaster(HIGHGUID_PLAYER, uint32(7));
    SpellEffectTriggerSpellContext ctx(target, caster, castItem, originalCaster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.m_CastItem == &castItem);
    CHECK(&ctx.m_originalCasterGUID == &originalCaster);

    ctx.unitTarget = reinterpret_cast<Unit*>(objects[3]);       // the unit target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<Unit*>(objects[3]));
    target = reinterpret_cast<Unit*>(objects[0]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[0]));
    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // so is the caster
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.m_CastItem = NULL;                                      // and the item it is cast with
    CHECK(castItem == NULL);
    castItem = reinterpret_cast<Item*>(objects[2]);
    CHECK(ctx.m_CastItem == reinterpret_cast<Item*>(objects[2]));
    ctx.m_originalCasterGUID = ObjectGuid(HIGHGUID_PLAYER, uint32(9)); // and the original caster
    CHECK(originalCaster == ObjectGuid(HIGHGUID_PLAYER, uint32(9)));
    originalCaster = ObjectGuid();
    CHECK(ctx.m_originalCasterGUID.IsEmpty());
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportRecallSiteHoldsItsThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));

    // Three labels, one body.
    typedef SpellHandler<SpellEffectTeleportRecallSite>::Function RecallBody;
    RecallBody recall = registry.Find<SpellEffectTeleportRecallSite>(48129);
    CHECK(recall != NULL);
    for (uint32 spellId : TELEPORT_RECALL_IDS)
    {
        CHECK(registry.Find<SpellEffectTeleportRecallSite>(spellId) == recall);
    }

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the teleport goes on with nothing
    // changed.
    Unit* target = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 23442);
    SpellEffectTeleportRecallContext recallCtx(target, spellInfo);
    CHECK(registry.FindDefault<SpellEffectTeleportRecallSite>() == NULL);
    CHECK(registry.Find<SpellEffectTeleportRecallSite>(23442) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTeleportRecallSite>(23442, recallCtx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(target == NULL);
    CHECK(spellInfo->ID == 23442);

    // A target that is not a player is never lost, whatever its level: each scroll goes on to the teleport.
    ValuedCreature targetUnit;
    target = &targetUnit;
    for (uint32 level : { 1u, 81u })
    {
        targetUnit.SetUInt32Value(UNIT_FIELD_LEVEL, level);
        for (uint32 spellId : TELEPORT_RECALL_IDS)
        {
            spellInfo = ObjectCombatSpell(0, spellId);
            SpellHandlerOutcome<void> scroll = registry.Dispatch<SpellEffectTeleportRecallSite>(spellId, recallCtx);
            CHECK(scroll.IsContinue());
            CHECK(!scroll.IsReturn());
        }
    }
    CHECK(target == &targetUnit);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));

    // Keyed on the recall site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TELEPORT_RECALL_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTeleportRecallSite::Key, spellId));
        CHECK(game.Find<SpellEffectTeleportRecallSite>(spellId) == recall);
    }
    for (uint32 spellId : { 23442u, 36941u, 36890u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectTeleportRecallSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTeleportRecallSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectTeleportRecallSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportRecallContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[2][16];
    Unit* target = reinterpret_cast<Unit*>(units[0]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 48129);
    SpellEffectTeleportRecallContext recallCtx(target, spellInfo);
    CHECK(&recallCtx.unitTarget == &target);
    CHECK(&recallCtx.m_spellInfo == &spellInfo);

    recallCtx.unitTarget = reinterpret_cast<Unit*>(units[1]);   // the unit target is the spell's member, not a copy
    CHECK(target == reinterpret_cast<Unit*>(units[1]));
    target = reinterpret_cast<Unit*>(units[0]);
    CHECK(recallCtx.unitTarget == reinterpret_cast<Unit*>(units[0]));
    recallCtx.m_spellInfo = ObjectCombatSpell(1, 60321);        // and so is the spell's entry
    CHECK(spellInfo->ID == 60321);
    spellInfo = ObjectCombatSpell(0, 60320);
    CHECK(recallCtx.m_spellInfo->ID == 60320);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportPostSiteHoldsItsThreeLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));

    // Three labels, three bodies; each rolls and casts on the caster, so none runs without a live unit.
    typedef SpellHandler<SpellEffectTeleportPostSite>::Function PostBody;
    std::set<PostBody> bodies;
    for (uint32 spellId : TELEPORT_POST_IDS)
    {
        bodies.insert(registry.Find<SpellEffectTeleportPostSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));

    // No default: an id with no row finds nothing, and its dispatch is a miss, so the member reaches its end with
    // nothing changed.
    Unit* caster = NULL;
    SpellEffectTeleportPostContext ctx(caster);
    CHECK(registry.FindDefault<SpellEffectTeleportPostSite>() == NULL);
    CHECK(registry.Find<SpellEffectTeleportPostSite>(48129) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectTeleportPostSite>(48129, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDamageTeleportHandlers(registry), DAMAGE_TELEPORT_ROWS);
    CHECK_EQ(registry.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));

    // Keyed on the post-teleport site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : TELEPORT_POST_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectTeleportPostSite::Key, spellId));
        CHECK(game.Find<SpellEffectTeleportPostSite>(spellId) == registry.Find<SpellEffectTeleportPostSite>(spellId));
    }
    for (uint32 spellId : { 48129u, 60320u, 60321u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectTeleportPostSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectTeleportPostSite::Key), std::size_t(3));
    CHECK(game.FindDefault<SpellEffectTeleportPostSite>() == NULL);
}

TEST(SpellEffectDamageTeleportHandlers_TheTeleportPostContextAliasesTheCaster)
{
    alignas(16) static unsigned char units[2][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    SpellEffectTeleportPostContext ctx(caster);
    CHECK(&ctx.m_caster == &caster);

    ctx.m_caster = reinterpret_cast<Unit*>(units[1]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[1]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
}

namespace
{
    // The five family switches' labels, in the switches' order.
    uint32 const DUMMY_MAGE_IDS[] = { 11958, 31687, 32826, 38194, 42955 };
    uint32 const DUMMY_WARRIOR_IDS[] = { 21977, 12975, 23881, 30012, 30284, 37144, 37146, 37148, 37151, 37152, 37153 };
    uint32 const DUMMY_ROGUE_IDS[] = { 5938, 14185, 31231, 51662 };
    uint32 const DUMMY_HUNTER_IDS[] = { 23989, 37506, 53478, 53271 };
    uint32 const DUMMY_PALADIN_IDS[] = { 19740, 20217, 31789, 37877 };

    // The six chess moves share one body.
    uint32 const DUMMY_CHESS_MOVE_IDS[] = { 37144, 37146, 37148, 37151, 37152, 37153 };

    // The file registers 5 + 11 + 4 + 4 + 4 rows and no default.
    uint32 const DUMMY_ROWS = 28;
}

TEST(SpellEffectDummyHandlers_TheMageSiteHoldsItsFiveLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));

    // Five labels, five bodies.
    typedef SpellHandler<SpellEffectDummyMageSite>::Function MageBody;
    std::set<MageBody> bodies;
    for (uint32 spellId : DUMMY_MAGE_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(5));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case goes on to the Conjure Mana Gem check with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 1);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(registry.FindDefault<SpellEffectDummyMageSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyMageSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyMageSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(ctx.spell == spell);
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // The bodies that need no live unit end the effect: Polymorph Cast Visual and Blink with no unit target, Cold
    // Snap for a caster that is not a player. Summon Water Elemental casts, and Conjure Refreshment reads the
    // target's level and creates an item through the spell; both need a live unit and a spell.
    for (uint32 spellId : { 32826u, 38194u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyMageSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    caster = &casterUnit;
    SpellHandlerOutcome<void> coldSnap = registry.Dispatch<SpellEffectDummyMageSite>(11958, ctx);
    CHECK(coldSnap.IsReturn());
    CHECK(!coldSnap.IsContinue());
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // Registering again on the same table changes nothing: every key is taken.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));

    // Keyed on the mage site only: its labels are no other site's, and the other sites' are not its; the game's
    // table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_MAGE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyMageSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyMageSite>(spellId) == registry.Find<SpellEffectDummyMageSite>(spellId));
    }
    for (uint32 spellId : { 21977u, 5938u, 23989u, 19740u, 24340u, 18461u, 66765u, 8342u })
    {
        CHECK(game.Find<SpellEffectDummyMageSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyMageSite::Key), std::size_t(5));
    CHECK(game.FindDefault<SpellEffectDummyMageSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheMageContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[5][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Unit* target = reinterpret_cast<Unit*>(objects[2]);
    int32 damage = 10;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyMageContext ctx(spell, caster, target, damage, effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose item creation reads damage
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[4]);       // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[4]));
    target = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[2]));
    ctx.damage = 20;                                            // and the damage, the stack size the spell reads
    CHECK_EQ(damage, int32(20));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}

TEST(SpellEffectDummyHandlers_TheWarriorSiteHoldsItsElevenLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));

    // Eleven labels, six bodies: the six chess moves share one; every other label has its own.
    typedef SpellHandler<SpellEffectDummyWarriorSite>::Function WarriorBody;
    WarriorBody chessMove = registry.Find<SpellEffectDummyWarriorSite>(37144);
    for (uint32 spellId : DUMMY_CHESS_MOVE_IDS)
    {
        CHECK(registry.Find<SpellEffectDummyWarriorSite>(spellId) == chessMove);
    }
    std::set<WarriorBody> bodies;
    for (uint32 spellId : DUMMY_WARRIOR_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyWarriorSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(6));
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(30012) != registry.Find<SpellEffectDummyWarriorSite>(30284));

    // No default: an id with no row (the hunter pet's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    int32 damage = 90;
    SpellEffectDummyWarriorContext ctx(caster, target, damage);
    CHECK(registry.FindDefault<SpellEffectDummyWarriorSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(53478) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyWarriorSite>(53478, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // With no unit target, Warrior's Wrath, Move (before it reaches Change Facing's body), Change Facing and the
    // chess moves end the effect. Last Stand and Bloodthirst cast and need a live unit.
    for (uint32 spellId : { 21977u, 30012u, 30284u, 37144u, 37146u, 37148u, 37151u, 37152u, 37153u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyWarriorSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(damage, int32(90));

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));
    CHECK(registry.Find<SpellEffectDummyWarriorSite>(37153) == chessMove);

    // Keyed on the warrior site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_WARRIOR_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyWarriorSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyWarriorSite>(spellId) == registry.Find<SpellEffectDummyWarriorSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 5938u, 53478u, 19740u, 20253u, 24340u, 18461u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyWarriorSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyWarriorSite::Key), std::size_t(11));
    CHECK(game.FindDefault<SpellEffectDummyWarriorSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheWarriorContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    int32 damage = 10;
    SpellEffectDummyWarriorContext ctx(caster, target, damage);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.damage == &damage);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    ctx.damage = 25;                                            // and the damage Bloodthirst passes on
    CHECK_EQ(damage, int32(25));
    damage = 40;
    CHECK_EQ(ctx.damage, int32(40));
}

TEST(SpellEffectDummyHandlers_TheRogueSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));

    // Four labels, four bodies.
    typedef SpellHandler<SpellEffectDummyRogueSite>::Function RogueBody;
    std::set<RogueBody> bodies;
    for (uint32 spellId : DUMMY_ROGUE_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyRogueSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(4));

    // No default: an id with no row (Readiness) finds nothing, and its dispatch is a miss, so the family's case
    // reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEffectDummyRogueContext ctx(caster, target);
    CHECK(registry.FindDefault<SpellEffectDummyRogueSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyRogueSite>(23989) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyRogueSite>(23989, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);

    // For a caster that is not a player, Shiv and Preparation end the effect. Cheat Death and Hunger for Blood cast
    // and need a live unit.
    Creature casterUnit(CREATURE_SUBTYPE_GENERIC);
    caster = &casterUnit;
    for (uint32 spellId : { 5938u, 14185u })
    {
        SpellHandlerOutcome<void> notPlayer = registry.Dispatch<SpellEffectDummyRogueSite>(spellId, ctx);
        CHECK(notPlayer.IsReturn());
        CHECK(!notPlayer.IsContinue());
    }
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));

    // Keyed on the rogue site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_ROGUE_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyRogueSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyRogueSite>(spellId) == registry.Find<SpellEffectDummyRogueSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 21977u, 23989u, 19740u, 18461u, 35729u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyRogueSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyRogueSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyRogueSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheRogueContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellEffectDummyRogueContext ctx(caster, target);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
}

TEST(SpellEffectDummyHandlers_TheHunterSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));

    // Four labels, four bodies.
    typedef SpellHandler<SpellEffectDummyHunterSite>::Function HunterBody;
    std::set<HunterBody> bodies;
    for (uint32 spellId : DUMMY_HUNTER_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyHunterSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(4));

    // No default: an id with no row (the warrior's Last Stand) finds nothing, and its dispatch is a miss, so the
    // family's case reaches its `break;` with nothing changed.
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 1);
    SpellEffectDummyHunterContext ctx(caster, target, effect);
    CHECK(registry.FindDefault<SpellEffectDummyHunterSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyHunterSite>(12975) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyHunterSite>(12975, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(effect->EffectIndex, uint32(1));

    // Last Stand with no unit target ends the effect; so do Readiness and Scatter Shot for a caster that is not a
    // player, and Master's Call for a caster with no pet.
    SpellHandlerOutcome<void> lastStand = registry.Dispatch<SpellEffectDummyHunterSite>(53478, ctx);
    CHECK(lastStand.IsReturn());
    CHECK(!lastStand.IsContinue());
    ValuedCreature casterUnit;
    caster = &casterUnit;
    for (uint32 spellId : { 23989u, 37506u, 53271u })
    {
        SpellHandlerOutcome<void> outcome = registry.Dispatch<SpellEffectDummyHunterSite>(spellId, ctx);
        CHECK(outcome.IsReturn());
        CHECK(!outcome.IsContinue());
    }
    CHECK(caster == &casterUnit);
    CHECK(target == NULL);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));

    // Keyed on the hunter site only; the game's table holds the same rows and no default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_HUNTER_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyHunterSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyHunterSite>(spellId) == registry.Find<SpellEffectDummyHunterSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 12975u, 5938u, 19740u, 57635u, 61507u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyHunterSite>(spellId) == NULL);
    }
    CHECK_EQ(game.CountAt(SpellEffectDummyHunterSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyHunterSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_TheHunterContextAliasesTheSpell)
{
    alignas(16) static unsigned char units[4][16];
    Unit* caster = reinterpret_cast<Unit*>(units[0]);
    Unit* target = reinterpret_cast<Unit*>(units[1]);
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyHunterContext ctx(caster, target, effect);
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(units[2]);           // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(units[2]));
    caster = reinterpret_cast<Unit*>(units[0]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(units[0]));
    ctx.unitTarget = reinterpret_cast<Unit*>(units[3]);         // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(units[3]));
    target = reinterpret_cast<Unit*>(units[1]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(units[1]));
    effect = WeaponDamageEffect(1, 2);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(2));
}

TEST(SpellEffectDummyHandlers_ThePaladinSiteHoldsItsFourLabelsAndNoDefault)
{
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.CountDefaults(), std::size_t(0));
    CHECK_EQ(registry.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));

    // Four labels, three bodies: the two blessings share one.
    typedef SpellHandler<SpellEffectDummyPaladinSite>::Function PaladinBody;
    PaladinBody blessing = registry.Find<SpellEffectDummyPaladinSite>(19740);
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20217) == blessing);
    std::set<PaladinBody> bodies;
    for (uint32 spellId : DUMMY_PALADIN_IDS)
    {
        bodies.insert(registry.Find<SpellEffectDummyPaladinSite>(spellId));
    }
    CHECK(bodies.count(NULL) == 0);
    CHECK_EQ(bodies.size(), std::size_t(3));

    // No default: an id with no row (Holy Shock's 20473, which the icon switch before it handles) finds nothing,
    // and its dispatch is a miss, so the family's case reaches its `break;` with nothing changed.
    alignas(16) static unsigned char objects[1][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = NULL;
    Unit* target = NULL;
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 19740);
    int32 basePoints[MAX_EFFECT_INDEX] = { 7, 8, 9 };
    Spell::TargetList targets = WeaponDamageTargets();
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyPaladinContext ctx(spell, caster, target, spellInfo, basePoints, targets, effect);
    CHECK(registry.FindDefault<SpellEffectDummyPaladinSite>() == NULL);
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20473) == NULL);
    SpellHandlerOutcome<void> miss = registry.Dispatch<SpellEffectDummyPaladinSite>(20473, ctx);
    CHECK(miss.IsMiss());
    CHECK(!miss.IsReturn());
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK(spellInfo->ID == 19740);
    CHECK_EQ(basePoints[0], int32(7));
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(targets.back().effectMask == 2);

    // With no unit target, the blessings and Blessing of Faith end the effect with nothing changed. Righteous
    // Defense answers through the spell's cast result and needs a live spell.
    for (uint32 spellId : { 19740u, 20217u, 37877u })
    {
        SpellHandlerOutcome<void> noTarget = registry.Dispatch<SpellEffectDummyPaladinSite>(spellId, ctx);
        CHECK(noTarget.IsReturn());
        CHECK(!noTarget.IsContinue());
    }
    CHECK(caster == NULL);
    CHECK(target == NULL);
    CHECK_EQ(targets.size(), std::size_t(3));
    CHECK(targets.back().effectMask == 2);

    // Registering again on the same table changes nothing.
    CHECK_EQ(RegisterSpellEffectDummyHandlers(registry), DUMMY_ROWS);
    CHECK_EQ(registry.Count(), std::size_t(DUMMY_ROWS));
    CHECK_EQ(registry.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));
    CHECK(registry.Find<SpellEffectDummyPaladinSite>(20217) == blessing);

    // Keyed on the paladin site only; the game's table holds the same rows and no default. 31789's own trigger,
    // 31980, stays at the trigger-spell site.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    for (uint32 spellId : DUMMY_PALADIN_IDS)
    {
        CHECK(!HeldByAnotherSite(game, SpellEffectDummyPaladinSite::Key, spellId));
        CHECK(game.Find<SpellEffectDummyPaladinSite>(spellId) == registry.Find<SpellEffectDummyPaladinSite>(spellId));
    }
    for (uint32 spellId : { 11958u, 21977u, 5938u, 23989u, 31980u, 20473u, 24340u, 66765u })
    {
        CHECK(game.Find<SpellEffectDummyPaladinSite>(spellId) == NULL);
    }
    CHECK(game.Find<SpellEffectTriggerSpellSite>(31980) != NULL);
    CHECK_EQ(game.CountAt(SpellEffectDummyPaladinSite::Key), std::size_t(4));
    CHECK(game.FindDefault<SpellEffectDummyPaladinSite>() == NULL);
}

TEST(SpellEffectDummyHandlers_ThePaladinContextAliasesTheSpell)
{
    alignas(16) static unsigned char objects[5][16];
    Spell* spell = reinterpret_cast<Spell*>(objects[0]);
    Unit* caster = reinterpret_cast<Unit*>(objects[1]);
    Unit* target = reinterpret_cast<Unit*>(objects[2]);
    SpellEntry const* spellInfo = ObjectCombatSpell(0, 31789);
    int32 basePoints[MAX_EFFECT_INDEX] = { 1, 2, 3 };
    Spell::TargetList targets;
    SpellEffectEntry const* effect = WeaponDamageEffect(0, 0);
    SpellEffectDummyPaladinContext ctx(spell, caster, target, spellInfo, basePoints, targets, effect);
    CHECK(ctx.spell == spell);                                  // the spell itself, whose methods the bodies call
    CHECK(&ctx.m_caster == &caster);
    CHECK(&ctx.unitTarget == &target);
    CHECK(&ctx.m_spellInfo == &spellInfo);
    CHECK(&ctx.m_currentBasePoints == &basePoints);
    CHECK(&ctx.m_UniqueTargetInfo == &targets);
    CHECK(&ctx.effect == &effect);

    ctx.m_caster = reinterpret_cast<Unit*>(objects[3]);         // the caster is the spell's member, not a copy
    CHECK(caster == reinterpret_cast<Unit*>(objects[3]));
    caster = reinterpret_cast<Unit*>(objects[1]);
    CHECK(ctx.m_caster == reinterpret_cast<Unit*>(objects[1]));
    ctx.unitTarget = reinterpret_cast<Unit*>(objects[4]);       // so is the unit target
    CHECK(target == reinterpret_cast<Unit*>(objects[4]));
    target = reinterpret_cast<Unit*>(objects[2]);
    CHECK(ctx.unitTarget == reinterpret_cast<Unit*>(objects[2]));
    ctx.m_spellInfo = ObjectCombatSpell(1, 37877);              // and the spell's entry
    CHECK(spellInfo->ID == 37877);
    spellInfo = ObjectCombatSpell(0, 19740);
    CHECK(ctx.m_spellInfo->ID == 19740);
    ctx.m_currentBasePoints[1] = 20217;                         // and its base points, the array itself
    CHECK_EQ(basePoints[1], int32(20217));
    basePoints[2] = 19740;
    CHECK_EQ(ctx.m_currentBasePoints[2], int32(19740));
    ctx.m_UniqueTargetInfo.push_back(Spell::TargetList::value_type()); // and its targets
    CHECK_EQ(targets.size(), std::size_t(1));
    targets.clear();
    CHECK(ctx.m_UniqueTargetInfo.empty());
    effect = WeaponDamageEffect(1, 1);                          // the effect is the member's parameter, read
    CHECK(ctx.effect == effect);
    CHECK_EQ(ctx.effect->EffectIndex, uint32(1));
}
