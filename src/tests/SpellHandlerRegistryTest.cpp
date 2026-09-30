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
/// contexts, and the game's sites (HandleAuraDummy's) are checked for their keys, their defaults
/// and their contexts, and run where a body needs no live Unit (the Improved Moonkin Form ranks,
/// the quest-tame labels, the Unrelenting Assault default); a body that casts needs a live Unit, which the harness
/// record covers where a scenario reaches it (931: 41101 and 53790, applied and removed; the coverage scenario
/// two-feigns-one-lift: the feign-death body, through 29266 and 31261; no scenario reaches a druid, quest-tame or
/// Unrelenting Assault label). Each dispatch mutant the note names has a test here that kills it:
///   lost key                 SpellHandlerRegistry_FindReturnsTheRegisteredFunction,
///                            AuraDummyHandlers_TheWarriorApplySiteHoldsTheSixStances,
///                            AuraDummyHandlers_TheRemoveSiteHoldsTheSixStancesAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheTableRegistersEveryRowOnce
///   wrong site               SpellHandlerRegistry_OneIdUnderTwoSitesIsTwoKeys,
///                            AuraDummyHandlers_TheGenericApplyRemoveSiteHoldsTheSixteenFeignDeathLabels,
///                            AuraDummyHandlers_TheRemoveSiteHoldsTheSixStancesAndNoDefault,
///                            AuraDummyHandlers_TheQuestTameSiteHoldsEighteenLabelsAndNoDefault
///   default first            SpellHandlerRegistry_TheDefaultRunsOnlyOnAMiss
///   default dropped          AuraDummyHandlers_TheUnrelentingAssaultSiteHoldsTwoRanksAndItsDefault,
///                            AuraDummyHandlers_TheImprovedMoonkinSiteHoldsThreeRanksAndItsDefault
///   Continue taken as Return SpellHandlerRegistry_ContinueAndReturnAreDistinct,
///                            AuraDummyHandlers_AContinueFromTheAssaultDefaultLeavesTheLoopNotTheFunction,
///                            AuraDummyHandlers_TheImprovedMoonkinRanksSetTheSpellTheTailCasts,
///                            AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts
///   a lost live-out          SpellHandlerRegistry_ALiveOutWrittenByAHandlerReachesTheSite,
///                            AuraDummyHandlers_TheApplyContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheRemoveContextAliasesTheTargetLocal,
///                            AuraDummyHandlers_TheNewContextsAliasTheirLocals,
///                            AuraDummyHandlers_TheQuestTameContextAliasesFinalSpellId
///   a rank's value changed   AuraDummyHandlers_TheQuestTameLabelsSetTheSpellTheTailCasts (all 18 id -> value pairs)

#include "TestHarness.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "spells/handlers/AuraDummyHandlers.h"
#include "Unit.h"                                               // SpellAuraProcResult

#include <list>
#include <set>
#include <string>

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

namespace
{
    typedef SpellHandler<AuraDummyUnrelentingAssaultSite>::Function AssaultFunction;
    typedef SpellHandler<AuraDummyImprovedMoonkinSite>::Function MoonkinFunction;

    alignas(16) unsigned char s_auraStorage[3][16];

    Aura* FakeAura(int i)
    {
        return reinterpret_cast<Aura*>(s_auraStorage[i]);
    }

    // The shape of the Unrelenting Assault site in HandleAuraDummy's Overpower block: a loop over
    // the caster's auras; at the one that passes the Unrelenting Assault test (here the second) the
    // dispatch stands where the switch stood, then `break;` leaves the loop and the function carries
    // on after it; a Return would have left the function. Returns the trace of what ran.
    std::string RunUnrelentingAssaultLoop(SpellHandlerRegistry const& registry, uint32 assaultId)
    {
        std::list<Aura*> modifierAuras;
        modifierAuras.push_back(FakeAura(0));
        modifierAuras.push_back(FakeAura(1));
        modifierAuras.push_back(FakeAura(2));
        Unit* target = NULL;
        std::string trace;
        for (std::list<Aura*>::const_iterator itr = modifierAuras.begin(); itr != modifierAuras.end(); ++itr)
        {
            trace += "aura;";
            if ((*itr) == FakeAura(1))
            {
                AuraDummyUnrelentingAssaultContext assaultCtx(target, itr);
                if (registry.Dispatch<AuraDummyUnrelentingAssaultSite>(assaultId, assaultCtx).IsReturn())
                {
                    trace += "returned;";
                    return trace;
                }
                trace += "after the switch;";
                break;
            }
        }
        trace += "after the loop;";
        return trace;
    }

    // The shape of the Improved Moonkin Form site: the rank's body sets `spell_id`, which the code
    // after the switch casts (or removes); a Return leaves before it. Returns what the tail reads,
    // or 0 when the site returned.
    uint32 RunImprovedMoonkin(SpellHandlerRegistry const& registry, uint32 rankId)
    {
        uint32 spell_id = 0;
        AuraDummyImprovedMoonkinContext imfCtx(NULL, spell_id);
        if (registry.Dispatch<AuraDummyImprovedMoonkinSite>(rankId, imfCtx).IsReturn())
        {
            return 0;
        }
        return spell_id;
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
    // 6 warrior stances, 2 Unrelenting Assault ranks and its default, 18 quest-tame labels, 6 stance removals,
    // 16 feign-death labels, 2 druid labels, 3 Improved Moonkin Form ranks and its default: 55 rows, 53 keys and
    // 2 defaults, none twice.
    SpellHandlerRegistry registry;
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(55));
    CHECK_EQ(registry.Count(), std::size_t(53));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraDummyApplyWarriorSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraDummyUnrelentingAssaultSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraDummyRemoveSite::Key), std::size_t(6));
    CHECK_EQ(registry.CountAt(AuraDummyQuestTameSite::Key), std::size_t(18));
    CHECK_EQ(registry.CountAt(AuraDummyApplyRemoveGenericSite::Key), std::size_t(16));
    CHECK_EQ(registry.CountAt(AuraDummyDruidSite::Key), std::size_t(2));
    CHECK_EQ(registry.CountAt(AuraDummyImprovedMoonkinSite::Key), std::size_t(3));

    // Registering again on the same table changes nothing: every key and default is taken.
    CHECK_EQ(RegisterAuraDummyHandlers(registry), uint32(55));
    CHECK_EQ(registry.Count(), std::size_t(53));
    CHECK_EQ(registry.CountDefaults(), std::size_t(2));

    // The game's table is the same one.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.Count(), std::size_t(53));
    CHECK_EQ(game.CountDefaults(), std::size_t(2));
}

TEST(AuraDummyHandlers_TheUnrelentingAssaultSiteHoldsTwoRanksAndItsDefault)
{
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    AssaultFunction rank1 = registry.Find<AuraDummyUnrelentingAssaultSite>(46859);
    AssaultFunction rank2 = registry.Find<AuraDummyUnrelentingAssaultSite>(46860);
    AssaultFunction onMiss = registry.FindDefault<AuraDummyUnrelentingAssaultSite>();
    CHECK(rank1 != NULL);
    CHECK(rank2 != NULL);
    CHECK(onMiss != NULL);
    CHECK(rank1 != rank2);
    CHECK(onMiss != rank1 && onMiss != rank2);

    // Keyed on the modifier aura's id, not the Overpower aura's: the ranks are not the warrior
    // apply site's, and that site's stances are not this one's.
    CHECK(registry.Find<AuraDummyApplyWarriorSite>(46859) == NULL);
    CHECK(registry.Find<AuraDummyUnrelentingAssaultSite>(41101) == NULL);

    // A miss runs the site's default (`default: break;`): Continue, never Miss.
    Unit* target = NULL;
    std::list<Aura*> auras(1, FakeAura(0));
    std::list<Aura*>::const_iterator itr = auras.begin();
    AuraDummyUnrelentingAssaultContext ctx(target, itr);
    CHECK(registry.Dispatch<AuraDummyUnrelentingAssaultSite>(41101, ctx).IsContinue());
    CHECK(registry.Dispatch<AuraDummyUnrelentingAssaultSite>(12345, ctx).IsContinue());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraDummyUnrelentingAssaultSite>(46859) == rank1);
    CHECK(game.Find<AuraDummyUnrelentingAssaultSite>(46860) == rank2);
    CHECK(game.FindDefault<AuraDummyUnrelentingAssaultSite>() == onMiss);
}

TEST(AuraDummyHandlers_AContinueFromTheAssaultDefaultLeavesTheLoopNotTheFunction)
{
    // The real default body, reached by a miss, answers Continue: the site's `break;` after the
    // switch leaves the loop and the code after the loop runs. (In HandleAuraDummy that code is a
    // `return;`, so there a Return would look the same; the double shows the outcome the site acts on.)
    CHECK_STR(RunUnrelentingAssaultLoop(SpellHandlerRegistry::Game(), 12345),
              std::string("aura;aura;after the switch;after the loop;"));

    // Against a Return handler at the same site, which leaves the function from inside the loop.
    SpellHandlerRegistry returning;
    CHECK(returning.RegisterDefault<AuraDummyUnrelentingAssaultSite>(
        [](AuraDummyUnrelentingAssaultContext&) { return SpellHandlerOutcome<void>::Return(); }));
    CHECK_STR(RunUnrelentingAssaultLoop(returning, 12345), std::string("aura;aura;returned;"));
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

    // An unmatched druid id is a Miss: HandleAuraDummy goes on to the Lifebloom, Predatory Strikes
    // and Improved Moonkin Form blocks after the switch (Lifebloom 33763, Improved Moonkin Form 48384).
    Unit* target = NULL;
    AuraDummyApplyRemoveContext ctx(NULL, target, true);
    CHECK(registry.Dispatch<AuraDummyDruidSite>(33763, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyDruidSite>(48384, ctx).IsMiss());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.Find<AuraDummyDruidSite>(52610) == savageRoar);
    CHECK(game.Find<AuraDummyDruidSite>(61336) == survivalInstincts);
}

TEST(AuraDummyHandlers_TheImprovedMoonkinSiteHoldsThreeRanksAndItsDefault)
{
    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    std::set<MoonkinFunction> distinct;
    static uint32 const ranks[] = { 48384, 48395, 48396 };
    for (uint32 rankId : ranks)
    {
        MoonkinFunction function = registry.Find<AuraDummyImprovedMoonkinSite>(rankId);
        CHECK(function != NULL);
        distinct.insert(function);
    }
    CHECK_EQ(distinct.size(), std::size_t(3));
    MoonkinFunction onMiss = registry.FindDefault<AuraDummyImprovedMoonkinSite>();
    CHECK(onMiss != NULL);                                      // a miss logs and returns (it needs the aura)
    CHECK(distinct.count(onMiss) == 0);
    CHECK(registry.Find<AuraDummyDruidSite>(48384) == NULL);    // the ranks are this site's only

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK(game.FindDefault<AuraDummyImprovedMoonkinSite>() == onMiss);
}

TEST(AuraDummyHandlers_TheImprovedMoonkinRanksSetTheSpellTheTailCasts)
{
    // The real rank bodies: each writes the block's `spell_id` and answers Continue, so the code
    // after the switch casts that spell.
    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(RunImprovedMoonkin(game, 48384), uint32(50170));
    CHECK_EQ(RunImprovedMoonkin(game, 48395), uint32(50171));
    CHECK_EQ(RunImprovedMoonkin(game, 48396), uint32(50172));
}

TEST(AuraDummyHandlers_TheNewContextsAliasTheirLocals)
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

    uint32 spell_id = 0;
    AuraDummyImprovedMoonkinContext moonkin(aura, spell_id);
    CHECK(moonkin.aura == aura);
    moonkin.spell_id = 50171;
    CHECK_EQ(spell_id, uint32(50171));

    // The Unrelenting Assault context holds the loop's iterator itself: the aura a body reads
    // through it is the one the loop stands on.
    std::list<Aura*> auras;
    auras.push_back(FakeAura(0));
    auras.push_back(FakeAura(1));
    std::list<Aura*>::const_iterator itr = auras.begin();
    AuraDummyUnrelentingAssaultContext assault(target, itr);
    CHECK((*assault.itr) == FakeAura(0));
    ++itr;
    CHECK((*assault.itr) == FakeAura(1));
    CHECK(assault.target == target);
}

TEST(AuraDummyHandlers_TheRemoveSiteHoldsTheSixStancesAndNoDefault)
{
    static uint32 const stances[] = { 41099, 41100, 41101, 53790, 53791, 53792 };

    SpellHandlerRegistry registry;
    RegisterAuraDummyHandlers(registry);
    for (uint32 spellId : stances)
    {
        SpellHandler<AuraDummyRemoveSite>::Function function = registry.Find<AuraDummyRemoveSite>(spellId);
        CHECK(function != NULL);

        // The same id at the warrior apply site is another key with another body.
        CHECK(registry.Find<AuraDummyApplyWarriorSite>(spellId) != NULL);
        CHECK(reinterpret_cast<void (*)()>(registry.Find<AuraDummyApplyWarriorSite>(spellId))
              != reinterpret_cast<void (*)()>(function));
    }

    // No default: an id the switch still holds (10255, 42454) or none misses, and that switch runs.
    CHECK(registry.FindDefault<AuraDummyRemoveSite>() == NULL);
    Unit* target = NULL;
    AuraDummyRemoveContext ctx(NULL, target);
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(10255, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(42454, ctx).IsMiss());
    CHECK(registry.Dispatch<AuraDummyRemoveSite>(11920, ctx).IsMiss());

    SpellHandlerRegistry const& game = SpellHandlerRegistry::Game();
    CHECK_EQ(game.CountAt(AuraDummyRemoveSite::Key), std::size_t(6));
    for (uint32 spellId : stances)
    {
        CHECK(game.Find<AuraDummyRemoveSite>(spellId) == registry.Find<AuraDummyRemoveSite>(spellId));
    }
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
