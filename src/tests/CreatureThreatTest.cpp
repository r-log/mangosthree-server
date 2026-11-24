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

/// Decoupling D5e (server #134): the Creature-only threat methods live on Creature.
///
/// The test is the static_asserts below, and they are the whole point: this file cannot
/// compile on master, where &Unit::SelectHostileTarget, &Unit::TauntApply and
/// &Unit::TauntFadeOut all name existing members. The runtime case underneath is
/// supplementary -- a bare Creature returned false from SelectHostileTarget() before the
/// move as well (its AI is null), so on its own it proves nothing about where the method
/// is declared.

#include "TestHarness.h"
#include "Creature.h"
#include "Unit.h"
#include "combat/HostileRefManager.h"
#include "combat/ThreatManager.h"
#include <type_traits>

namespace
{
    // Each trait is true only when &T::<name> names a member of T itself or of a base of
    // T. After the move, Creature declares all three and Unit declares none, so every pair
    // below holds; before the move, Unit declared all three and Creature inherited them, so
    // the three !Has...<Unit> asserts all fail.
    template <typename T, typename = void>
    struct HasSelectHostileTarget : std::false_type {};
    template <typename T>
    struct HasSelectHostileTarget<T, std::void_t<decltype(&T::SelectHostileTarget)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasTauntApply : std::false_type {};
    template <typename T>
    struct HasTauntApply<T, std::void_t<decltype(&T::TauntApply)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasTauntFadeOut : std::false_type {};
    template <typename T>
    struct HasTauntFadeOut<T, std::void_t<decltype(&T::TauntFadeOut)>> : std::true_type {};

    static_assert(!HasSelectHostileTarget<Unit>::value, "SelectHostileTarget must not be declared on Unit");
    static_assert(HasSelectHostileTarget<Creature>::value, "SelectHostileTarget must be declared on Creature");

    static_assert(!HasTauntApply<Unit>::value, "TauntApply must not be declared on Unit");
    static_assert(HasTauntApply<Creature>::value, "TauntApply must be declared on Creature");

    static_assert(!HasTauntFadeOut<Unit>::value, "TauntFadeOut must not be declared on Unit");
    static_assert(HasTauntFadeOut<Creature>::value, "TauntFadeOut must be declared on Creature");

    // The trait itself has to be right or the asserts above are vacuous. These two pin it
    // against a member that did not move and one that never existed.
    template <typename T, typename = void>
    struct HasFixateTarget : std::false_type {};
    template <typename T>
    struct HasFixateTarget<T, std::void_t<decltype(&T::FixateTarget)>> : std::true_type {};

    template <typename T, typename = void>
    struct HasNoSuchMethod : std::false_type {};
    template <typename T>
    struct HasNoSuchMethod<T, std::void_t<decltype(&T::ThisMemberDoesNotExist)>> : std::true_type {};

    static_assert(HasFixateTarget<Unit>::value, "FixateTarget stays on Unit: its callers hold a Unit*");
    static_assert(HasFixateTarget<Creature>::value, "Creature still inherits FixateTarget from Unit");
    static_assert(!HasNoSuchMethod<Unit>::value, "the detection trait must report a missing member as absent");
    static_assert(!HasNoSuchMethod<Creature>::value, "the detection trait must report a missing member as absent");
}

TEST(CreatureThreat_BareCreatureSelectsNoHostileTarget)
{
    // Supplementary runtime check: a bare Creature (no map, no AI, empty threat list) is
    // alive (Unit::Unit sets m_deathState = ALIVE) and so reaches the AI test, where i_AI
    // is NULL (Creature::Creature) and the method returns false without touching a map.
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    CHECK(!creature.SelectHostileTarget());
}

/// Threat held off a hostile reference (HostileReference::AddThreatReduction) and shown again, on bare creatures
/// (update fields allocated; no map, template or AI): one creature is the unit held, each other one lists it with
/// ThreatManager::addThreat. The holder is declared before the creatures that list it, so they let go of it first.
namespace
{
    class ThreatTestCreature : public Creature
    {
        public:
            ThreatTestCreature() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            ~ThreatTestCreature()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    const float kAsk = 90000000.0f;     // Mirror Image's, Fade's and Shadowmeld's aura 103 amount, as held off

    float Shown(ThreatTestCreature& lister, ThreatTestCreature& held)
    {
        return lister.GetThreatManager().getThreat(&held);
    }
}

TEST(CreatureThreat_AnAskHoldsTheThreatEarnedUnderItAndShowsItWhole)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);
    REQUIRE(Shown(camel, mage) == 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);
    CHECK(!camel.GetThreatManager().isThreatListEmpty());

    camel.GetThreatManager().addThreat(&mage, 300.0f);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 5300.0f);
}

TEST(CreatureThreat_TwoAsksAreWithdrawnOneAtATime)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 5000.0f);
}

TEST(CreatureThreat_AnAskOnNoThreatChangesNothing)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 0.0f);
    REQUIRE(!camel.GetThreatManager().isThreatListEmpty());

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 0.0f);
}

TEST(CreatureThreat_AnAskSmallerThanTheThreatHoldsOffItsAmount)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 4000.0f);

    camel.GetThreatManager().addThreat(&mage, 300.0f);
    CHECK_EQ(Shown(camel, mage), 4300.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 5300.0f);
}

TEST(CreatureThreat_APercentUnderAnAskTakesItFromTheThreatEarned)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);

    mage.GetHostileRefManager().AddThreatReduction(kAsk);
    mage.GetHostileRefManager().addThreatPercent(-10);
    CHECK_EQ(Shown(camel, mage), 0.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(kAsk);
    CHECK_EQ(Shown(camel, mage), 4500.0f);
}

TEST(CreatureThreat_TheTauntTemporaryUnderAnAskMovesTheThreatEarned)
{
    ThreatTestCreature mage;
    ThreatTestCreature camel;
    camel.GetThreatManager().addThreat(&mage, 5000.0f);
    HostileReference* ref = mage.GetHostileRefManager().getFirst();
    REQUIRE(ref);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    REQUIRE(Shown(camel, mage) == 4000.0f);

    ref->setTempThreat(10000.0f);
    CHECK_EQ(Shown(camel, mage), 10000.0f);

    ref->resetTempThreat();
    CHECK_EQ(Shown(camel, mage), 4000.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(camel, mage), 5000.0f);
}

TEST(CreatureThreat_TheManagerAsksEveryListAndALaterListIsUntouched)
{
    ThreatTestCreature mage;
    ThreatTestCreature first;
    ThreatTestCreature second;
    ThreatTestCreature third;
    ThreatTestCreature later;
    first.GetThreatManager().addThreat(&mage, 5000.0f);
    second.GetThreatManager().addThreat(&mage, 700.0f);
    third.GetThreatManager().addThreat(&mage, 2500.0f);

    mage.GetHostileRefManager().AddThreatReduction(1000.0f);
    CHECK_EQ(Shown(first, mage), 4000.0f);
    CHECK_EQ(Shown(second, mage), 0.0f);
    CHECK_EQ(Shown(third, mage), 1500.0f);

    later.GetThreatManager().addThreat(&mage, 600.0f);
    CHECK_EQ(Shown(later, mage), 600.0f);

    mage.GetHostileRefManager().RemoveThreatReduction(1000.0f);
    CHECK_EQ(Shown(first, mage), 5000.0f);
    CHECK_EQ(Shown(second, mage), 700.0f);
    CHECK_EQ(Shown(third, mage), 2500.0f);
    CHECK_EQ(Shown(later, mage), 600.0f);
}
