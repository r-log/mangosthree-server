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

/// The rank Unit's proc handlers ask a player for: its rank with a faction, by which the Shattered
/// Sun pendants' proc picks the Aldor's or the Scryers' spell.
///
/// Unit declares it public: the proc handlers ask it of the unit they run for, and a call through
/// a Unit reference compiles. A Unit that is not a Player answers with Unit's default: a bare
/// Creature probe (no map, no AI, no auras) is asked through a Unit reference. A Player cannot be
/// built in this binary (it needs a WorldSession and a map); the static_asserts pin Unit's
/// declaration through the probe, that Player declares it public with Unit's exact signature, and
/// which references a call compiles through; Player.h marks it `override final`, so a Player
/// answers with its own.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "ReputationMgr.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    /// A bare Creature.
    class RankedUnit : public Creature
    {
        public:
            static_assert(std::is_same<decltype(&RankedUnit::GetReputationRank),
                                       ReputationRank (Unit::*)(uint32) const>::value,
                          "Unit declares the rank with a faction");

            RankedUnit() : Creature(CREATURE_SUBTYPE_GENERIC) { }
    };

    template<class T>
    auto CallsReputationRank(int)
        -> decltype(std::declval<T const&>().GetReputationRank(uint32(0)), std::true_type());
    template<class T>
    std::false_type CallsReputationRank(...);
}

static_assert(std::is_same<decltype(&Player::GetReputationRank), ReputationRank (Player::*)(uint32) const>::value,
              "Player returns its rank by its reputation with the faction");

static_assert(decltype(CallsReputationRank<Unit>(0))::value,
              "Unit's rank is public: a call through a Unit compiles");
static_assert(decltype(CallsReputationRank<Player>(0))::value,
              "Player's rank is public: a call through a Player compiles");

TEST(UnitReputationRank_ACreatureIsNeutralWithEveryFaction)
{
    RankedUnit creature;
    Unit const& unit = creature;

    CHECK_EQ(int(unit.GetReputationRank(932)), int(REP_NEUTRAL));
    CHECK_EQ(int(unit.GetReputationRank(934)), int(REP_NEUTRAL));
    CHECK_EQ(int(unit.GetReputationRank(0)), int(REP_NEUTRAL));
    CHECK_EQ(int(unit.GetReputationRank(0xFFFFFFFF)), int(REP_NEUTRAL));
}

TEST(UnitReputationRank_ACreatureAnswersAsAReputationWithNoStanding)
{
    RankedUnit creature;
    Unit const& unit = creature;

    CHECK_EQ(int(unit.GetReputationRank(932)), int(ReputationMgr::ReputationToRank(0)));
}
