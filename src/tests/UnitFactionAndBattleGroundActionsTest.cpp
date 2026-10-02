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

/// The faction a player returns to and the battleground test Unit asks a player for:
/// RestoreOriginalFaction returns a player to its race's faction when a faction override ends, and
/// UpdateSpeed picks a dead player's ghost run speed by whether it is in a battleground.
///
/// Unit declares both protected, with no default arguments. A Unit that is not a Player sets no
/// faction for a race and is in no battleground: a Creature probe with its update fields allocated
/// (no map, no AI, no auras) makes the two public with using-declarations and is asked through its
/// own reference; the faction for every race leaves the faction template and every other update
/// field as they were, and the battleground test answers false. A Player cannot be built in this
/// binary (it needs a WorldSession and a map); the static_asserts pin Unit's declarations through
/// the probe, that Player declares both public with Unit's exact signature, the test `const` on
/// both sides, and which references a call compiles through. That Player's test is `final` is
/// pinned by the cast proof's text of Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated and a faction set; the faction for a race and
    /// the battleground test are public here.
    class FactionUnit : public Creature
    {
        public:
            using Unit::setFactionForRace;
            using Unit::InBattleGround;

            static_assert(std::is_same<decltype(&FactionUnit::setFactionForRace), void (Unit::*)(uint8)>::value,
                          "Unit declares the faction for a race");
            static_assert(std::is_same<decltype(&FactionUnit::InBattleGround), bool (Unit::*)() const>::value,
                          "Unit declares the battleground test, const");

            explicit FactionUnit(uint32 faction) : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                setFaction(faction);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~FactionUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }

            std::vector<uint32> Fields() const
            {
                std::vector<uint32> fields;
                for (uint16 i = 0; i < GetValuesCount(); ++i)
                {
                    fields.push_back(GetUInt32Value(i));
                }
                return fields;
            }
    };

    template<class T>
    auto CallsSetFactionForRace(int)
        -> decltype(std::declval<T&>().setFactionForRace(uint8(RACE_HUMAN)), std::true_type());
    template<class T>
    std::false_type CallsSetFactionForRace(...);

    template<class T>
    auto CallsInBattleGround(int) -> decltype(std::declval<T const&>().InBattleGround(), std::true_type());
    template<class T>
    std::false_type CallsInBattleGround(...);
}

static_assert(std::is_same<decltype(&Player::setFactionForRace), void (Player::*)(uint8)>::value,
              "Player sets its team and the faction template of the race");
static_assert(std::is_same<decltype(&Player::InBattleGround), bool (Player::*)() const>::value,
              "Player answers by its battleground instance, const");

static_assert(!decltype(CallsSetFactionForRace<Unit>(0))::value,
              "Unit's faction for a race is protected: a call through a Unit does not compile");
static_assert(decltype(CallsSetFactionForRace<Player>(0))::value,
              "Player's faction for a race is public: a call through a Player compiles");
static_assert(!decltype(CallsInBattleGround<Unit>(0))::value,
              "Unit's battleground test is protected: a call through a Unit does not compile");
static_assert(decltype(CallsInBattleGround<Player>(0))::value,
              "Player's battleground test is public: a call through a const Player compiles");

TEST(UnitFactionAndBattleGroundActions_ACreatureKeepsItsFactionForEveryRace)
{
    FactionUnit creature(14);
    FactionUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());
    CHECK_EQ(creature.getFaction(), uint32(14));

    for (uint32 race = 0; race <= MAX_RACES; ++race)
    {
        unit.setFactionForRace(uint8(race));
        CHECK_EQ(creature.getFaction(), uint32(14));
        CHECK(creature.Fields() == fields);
    }
    unit.setFactionForRace(uint8(255));
    CHECK_EQ(creature.getFaction(), uint32(14));
    CHECK(creature.Fields() == fields);
}

TEST(UnitFactionAndBattleGroundActions_ACreatureIsInNoBattleGround)
{
    FactionUnit creature(14);
    FactionUnit const& unit = creature;
    std::vector<uint32> const fields = creature.Fields();

    CHECK(!unit.InBattleGround());

    CHECK(creature.Fields() == fields);
}
