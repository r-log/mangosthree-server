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
