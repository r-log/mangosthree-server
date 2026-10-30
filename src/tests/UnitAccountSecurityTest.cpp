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

/// The security level of the account playing a unit, which Unit's GM visibility rule and a pet's
/// talent reset compare: a game master in GM mode sees a player whose level is not above its own,
/// and a pet keeps its talents for an administrator.
///
/// Unit declares it public, with no argument. A Unit that is not a Player has no account: a bare
/// Creature probe (no map, no AI, no auras) is asked through a Unit reference. Player's override
/// reads the query its session installed, which the client packets test asks. Player's override
/// is private: the static_asserts pin Unit's declaration, Player's own declaration with Unit's
/// exact signature (named in an explicit instantiation, where access is not checked), that a call
/// through a Unit compiles, and that a call through a Player does not.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    static_assert(std::is_same<decltype(&Unit::GetAccountSecurityLevel), uint32 (Unit::*)() const>::value,
                  "Unit declares the account security level, const");

    /// Checks the type of the member it is instantiated with. Player's override is private, so it
    /// is named only in the explicit instantiation below: a member Player does not declare itself
    /// has Unit's member pointer type, which fails the check, and an overloaded name has no type.
    template<class Member, Member member>
    struct PlayerAccountSecurityDeclaration
    {
        static_assert(std::is_same<Member, uint32 (Player::*)() const>::value,
                      "Player declares the account security level with Unit's signature, const");
    };

    template struct PlayerAccountSecurityDeclaration<decltype(&Player::GetAccountSecurityLevel),
                                                     &Player::GetAccountSecurityLevel>;

    template<class T>
    auto AsksAccountSecurity(int)
        -> decltype(std::declval<T const&>().GetAccountSecurityLevel(), std::true_type());
    template<class T>
    std::false_type AsksAccountSecurity(...);
}

static_assert(decltype(AsksAccountSecurity<Unit>(0))::value,
              "Unit's account security level is public: a call through a Unit compiles");
static_assert(!decltype(AsksAccountSecurity<Player>(0))::value,
              "Player's account security level is private: a call through a Player does not compile");

TEST(UnitAccountSecurity_ACreatureHasAPlayersLevel)
{
    Creature creature(CREATURE_SUBTYPE_GENERIC);
    Unit const& unit = creature;

    CHECK_EQ(unit.GetAccountSecurityLevel(), uint32(0));
}
