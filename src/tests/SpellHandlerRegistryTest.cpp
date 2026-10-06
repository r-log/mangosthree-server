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

/// Decoupling D11 (design/2026-09-28-unit-reopening.md 3(b)): the spell handler registry.
///
/// The registry is tested without a map: its sites here are test sites with their own keys and
/// contexts, and the game's sites (HandleAuraDummy's and HandleAuraTransform's) are checked for their keys, their
/// defaults and their contexts, and run where a body needs no live Unit (the quest-tame labels, the removal labels on
/// a mode that keeps them off the Unit); a body that casts or sets a display needs a live Unit, which the harness
/// record covers where a scenario reaches it (931: 41101 and 53790, applied and removed; the coverage scenario
/// two-feigns-one-lift: the feign-death body, through 29266 and 31261; no scenario reaches a druid, quest-tame or
/// transform label, nor another removal one).
/// Each dispatch mutant the note names has a test here that kills it:
///   lost key                 SpellHandlerRegistry_FindReturnsTheRegisteredFunction,
///                            AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheTableRegistersEveryRowOnce,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault
///   wrong site               SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheRemoveSiteHoldsItsThirtyLabelsAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraShapeshiftHandlers_TheTransformSiteHoldsItsNineLabelsAndTheDefault
///   default first            SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss
///   Continue taken as Return SpellHandlerRegistry_ContinueAndReturnAreDistinct,
///                            AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts
///   a lost live-out          SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite,
///                            AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheApplyRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId,
///                            AuraShapeshiftHandlers_TheTransformContextAliasesTheTargetLocal
///   a rank's value changed   AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts (all 18 id -> value pairs)
///   a stale removal mode     AuraDummyHandlers_ARemovalBodyReadsTheModeWhenItRuns

#include "TestHarness.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "spells/handlers/AuraShapeshiftHandlers.h"
#include "Unit.h"                                               // SpellAuraProcResult
#include "SpellAuras.h"

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

    // The game's table holds these and the transform site's 9 rows and default.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(81));
    CHECK_EQ(game.CountDefaults(), std::size_t(1));
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
