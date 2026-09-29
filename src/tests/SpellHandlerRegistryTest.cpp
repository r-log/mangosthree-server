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
/// contexts, and the game's one site (HandleAuraDummy's warrior apply switch) is checked for its
/// keys and its context only -- running a warrior handler needs a live Unit, which the harness
/// record (scenario 931) covers. Each dispatch mutant the note names has a test here that kills it:
///   lost key                 SpellHandlerRegistry_FindReturnsTheRegisteredFunction,
///                            AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances
///   wrong site               SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys
///   default first            SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss
///   Continue taken as Return SpellHandlerRegistry_ContinueAndReturnAreDistinct
///   a lost live-out          SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite,
///                            AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal

#include "TestHarness.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "Unit.h"                                               // SpellAuraProcResult

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
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(6));
    CHECK_EQ(registry.Count(), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    std::set<SpellHandler<AuraDummyApplyWarriorSite>::Function> distinct;
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyApplyWarriorSite>::Function function = registry.Find<AuraDummyApplyWarriorSite>(spellId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(6));                  // six bodies, none shared

    // The family's other switch (Unrelenting Assault, keyed on another aura's id) is not this site,
    // and the site has no default: a miss is Miss() and the Overpower block runs after it.
    CHECK(registry.Find<AuraDummyApplyWarriorSite>(46859) == NULL);
    CHECK(registry.FindDefault<AuraDummyApplyWarriorSite>() == NULL);
    Unit* target = NULL;
    AuraDummyApplyContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyApplyWarriorSite>(46859, ctx).IsMiss());

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
