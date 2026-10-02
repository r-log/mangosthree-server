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

/// The rank of a talent Unit's spell bonus asks a player for: SpellBonusWithCoeffs raises a death
/// knight's attack power bonus by the rank of Impurity (talent 2005) it knows.
///
/// Unit declares it protected, with no default argument. A Unit that is not a Player knows no
/// talent: a bare Creature probe (no map, no AI, no auras) makes it public with a
/// using-declaration and is asked through its own reference. A Player cannot be built in this
/// binary (it needs a WorldSession and a map). Player's override is private: the static_asserts pin
/// Unit's declaration through the probe, Player's own declaration with Unit's exact signature (named
/// in an explicit instantiation, where access is not checked), and that a call through a Unit or
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
    /// A bare Creature; the talent rank Unit asks a player for is public here.
    class TalentRankUnit : public Creature
    {
        public:
            using Unit::GetKnownTalentRankById;

            static_assert(std::is_same<decltype(&TalentRankUnit::GetKnownTalentRankById),
                                       SpellEntry const* (Unit::*)(int32) const>::value,
                          "Unit declares the talent rank, const");

            TalentRankUnit() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    /// Checks the type of the member it is instantiated with. Player's override is private, so it
    /// is named only in the explicit instantiation below: a member Player does not declare itself
    /// is Unit's and does not convert to a Player member, and an overloaded name has no type.
    template<class Member, Member member>
    struct PlayerTalentRankDeclaration
    {
        static_assert(std::is_same<Member, SpellEntry const* (Player::*)(int32) const>::value,
                      "Player declares the talent rank with Unit's signature, const");
    };

    template struct PlayerTalentRankDeclaration<decltype(&Player::GetKnownTalentRankById),
                                                &Player::GetKnownTalentRankById>;

    template<class T>
    auto CallsKnownTalentRank(int)
        -> decltype(std::declval<T const&>().GetKnownTalentRankById(int32(0)), std::true_type());
    template<class T>
    std::false_type CallsKnownTalentRank(...);
}

static_assert(decltype(CallsKnownTalentRank<TalentRankUnit>(0))::value,
              "a call through the probe reaches Unit's talent rank");
static_assert(!decltype(CallsKnownTalentRank<Unit>(0))::value,
              "Unit's talent rank is protected: a call through a Unit does not compile");
static_assert(!decltype(CallsKnownTalentRank<Player>(0))::value,
              "Player's talent rank is private: a call through a Player does not compile");

TEST(UnitTalentRank_ACreatureKnowsNoTalent)
{
    TalentRankUnit creature;
    TalentRankUnit const& unit = creature;

    CHECK(unit.GetKnownTalentRankById(2005) == NULL);
    CHECK(unit.GetKnownTalentRankById(1800) == NULL);
    CHECK(unit.GetKnownTalentRankById(0) == NULL);
    CHECK(unit.GetKnownTalentRankById(-1) == NULL);
}
