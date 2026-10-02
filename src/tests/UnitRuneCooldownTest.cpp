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

/// Whether every base rune of a type is on cooldown, which Unit's Blade Barrier proc asks a player:
/// the proc fires only for a death knight whose base blood runes are all on cooldown.
///
/// Unit declares it protected, with no default argument. A Unit that is not a Player has no runes:
/// a bare Creature probe (no map, no AI, no auras) makes it public with a using-declaration and is
/// asked through its own reference. A Player cannot be built in this binary (it needs a
/// WorldSession and a map). Player's override is private: the static_asserts pin Unit's
/// declaration through the probe, Player's own declaration with Unit's exact signature (named in
/// an explicit instantiation, where access is not checked), and that a call through a Unit or
/// through a Player does not compile. That Player's override is `final` is pinned by the cast
/// proof's text of Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    /// A bare Creature; the rune cooldown test Unit asks a player for is public here.
    class RuneCooldownUnit : public Creature
    {
        public:
            using Unit::IsBaseRuneSlotsOnCooldown;

            static_assert(std::is_same<decltype(&RuneCooldownUnit::IsBaseRuneSlotsOnCooldown),
                                       bool (Unit::*)(RuneType) const>::value,
                          "Unit declares the rune cooldown test, const");

            RuneCooldownUnit() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    /// Checks the type of the member it is instantiated with. Player's override is private, so it
    /// is named only in the explicit instantiation below: a member Player does not declare itself
    /// has Unit's member pointer type, which fails the check, and an overloaded name has no type.
    template<class Member, Member member>
    struct PlayerRuneCooldownDeclaration
    {
        static_assert(std::is_same<Member, bool (Player::*)(RuneType) const>::value,
                      "Player declares the rune cooldown test with Unit's signature, const");
    };

    template struct PlayerRuneCooldownDeclaration<decltype(&Player::IsBaseRuneSlotsOnCooldown),
                                                  &Player::IsBaseRuneSlotsOnCooldown>;

    template<class T>
    auto CallsRuneCooldown(int)
        -> decltype(std::declval<T const&>().IsBaseRuneSlotsOnCooldown(RUNE_BLOOD), std::true_type());
    template<class T>
    std::false_type CallsRuneCooldown(...);
}

static_assert(decltype(CallsRuneCooldown<RuneCooldownUnit>(0))::value,
              "a call through the probe reaches Unit's rune cooldown test");
static_assert(!decltype(CallsRuneCooldown<Unit>(0))::value,
              "Unit's rune cooldown test is protected: a call through a Unit does not compile");
static_assert(!decltype(CallsRuneCooldown<Player>(0))::value,
              "Player's rune cooldown test is private: a call through a Player does not compile");

TEST(UnitRuneCooldown_ACreatureHasNoRuneOnCooldown)
{
    RuneCooldownUnit creature;
    RuneCooldownUnit const& unit = creature;

    CHECK(!unit.IsBaseRuneSlotsOnCooldown(RUNE_BLOOD));
    CHECK(!unit.IsBaseRuneSlotsOnCooldown(RUNE_UNHOLY));
    CHECK(!unit.IsBaseRuneSlotsOnCooldown(RUNE_FROST));
    CHECK(!unit.IsBaseRuneSlotsOnCooldown(RUNE_DEATH));
    CHECK(!unit.IsBaseRuneSlotsOnCooldown(NUM_RUNE_TYPES));
}
