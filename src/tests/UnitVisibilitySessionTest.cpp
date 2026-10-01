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

/// What Unit's visibility, targeting and aura checks ask of a player: whether it is a game master,
/// whether its session is loading it or logging it out, the transport it rides, whether a group
/// member sees it through stealth, and its drunk value.
///
/// Unit declares the six protected: its own checks ask them, and a call through a Unit reference
/// from outside does not compile. A Unit that is not a Player answers with Unit's defaults: a
/// Creature probe (no map, no AI, no auras) makes the six public with using-declarations and is
/// asked through its own reference. A Player cannot be built in this binary (it needs a
/// WorldSession and a map); the static_asserts pin Unit's declarations through the probe, that
/// Player declares each of the six public with Unit's exact signature, and which references a
/// call compiles through; Player.h marks each `override`, so a Player answers with its own.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    /// A Creature with its update fields allocated and every unit flag and byte field set; the six
    /// Unit asks a player are public here.
    class SeenUnit : public Creature
    {
        public:
            using Unit::isGameMaster;
            using Unit::IsLoading;
            using Unit::IsLoggingOut;
            using Unit::GetTransport;
            using Unit::IsGroupVisibleFor;
            using Unit::GetDrunkValue;

            static_assert(std::is_same<decltype(&SeenUnit::isGameMaster), bool (Unit::*)() const>::value,
                          "Unit declares the game master question");
            static_assert(std::is_same<decltype(&SeenUnit::IsLoading), bool (Unit::*)() const>::value,
                          "Unit declares the loading question");
            static_assert(std::is_same<decltype(&SeenUnit::IsLoggingOut), bool (Unit::*)() const>::value,
                          "Unit declares the logging-out question");
            static_assert(std::is_same<decltype(&SeenUnit::GetTransport), Transport* (Unit::*)() const>::value,
                          "Unit declares the transport it rides");
            static_assert(std::is_same<decltype(&SeenUnit::IsGroupVisibleFor), bool (Unit::*)(Player*) const>::value,
                          "Unit declares the group visibility question");
            static_assert(std::is_same<decltype(&SeenUnit::GetDrunkValue), uint16 (Unit::*)() const>::value,
                          "Unit declares the drunk value");

            SeenUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                SetUInt32Value(UNIT_FIELD_FLAGS, 0xFFFFFFFF);
                SetUInt32Value(UNIT_FIELD_BYTES_0, 0xFFFFFFFF);
                SetUInt32Value(UNIT_FIELD_BYTES_1, 0xFFFFFFFF);
                SetUInt32Value(UNIT_FIELD_BYTES_2, 0xFFFFFFFF);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~SeenUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    template<class T>
    auto CallsGameMaster(int) -> decltype(std::declval<T const&>().isGameMaster(), std::true_type());
    template<class T>
    std::false_type CallsGameMaster(...);

    template<class T>
    auto CallsLoading(int) -> decltype(std::declval<T const&>().IsLoading(), std::true_type());
    template<class T>
    std::false_type CallsLoading(...);

    template<class T>
    auto CallsLoggingOut(int) -> decltype(std::declval<T const&>().IsLoggingOut(), std::true_type());
    template<class T>
    std::false_type CallsLoggingOut(...);

    template<class T>
    auto CallsTransport(int) -> decltype(std::declval<T const&>().GetTransport(), std::true_type());
    template<class T>
    std::false_type CallsTransport(...);

    template<class T>
    auto CallsGroupVisible(int)
        -> decltype(std::declval<T const&>().IsGroupVisibleFor(static_cast<Player*>(NULL)), std::true_type());
    template<class T>
    std::false_type CallsGroupVisible(...);

    template<class T>
    auto CallsDrunkValue(int) -> decltype(std::declval<T const&>().GetDrunkValue(), std::true_type());
    template<class T>
    std::false_type CallsDrunkValue(...);
}

static_assert(std::is_same<decltype(&Player::isGameMaster), bool (Player::*)() const>::value,
              "Player answers from its game master flag");
static_assert(std::is_same<decltype(&Player::IsLoading), bool (Player::*)() const>::value,
              "Player asks its session whether it is loading");
static_assert(std::is_same<decltype(&Player::IsLoggingOut), bool (Player::*)() const>::value,
              "Player asks its session whether it is logging out");
static_assert(std::is_same<decltype(&Player::GetTransport), Transport* (Player::*)() const>::value,
              "Player returns the transport it rides");
static_assert(std::is_same<decltype(&Player::IsGroupVisibleFor), bool (Player::*)(Player*) const>::value,
              "Player answers under the group visibility setting");
static_assert(std::is_same<decltype(&Player::GetDrunkValue), uint16 (Player::*)() const>::value,
              "Player returns its own drunk value");

static_assert(!decltype(CallsGameMaster<Unit>(0))::value && !decltype(CallsLoading<Unit>(0))::value
              && !decltype(CallsLoggingOut<Unit>(0))::value && !decltype(CallsTransport<Unit>(0))::value
              && !decltype(CallsGroupVisible<Unit>(0))::value && !decltype(CallsDrunkValue<Unit>(0))::value,
              "Unit's six are protected: a call through a Unit does not compile");
static_assert(decltype(CallsGameMaster<Player>(0))::value && decltype(CallsLoading<Player>(0))::value
              && decltype(CallsLoggingOut<Player>(0))::value && decltype(CallsTransport<Player>(0))::value
              && decltype(CallsGroupVisible<Player>(0))::value && decltype(CallsDrunkValue<Player>(0))::value,
              "Player's six are public: a call through a Player compiles");

TEST(UnitVisibilitySession_ACreatureIsNoGameMasterAndHasNoSession)
{
    SeenUnit creature;
    SeenUnit const& unit = creature;

    CHECK(!unit.isGameMaster());
    CHECK(!unit.IsLoading());
    CHECK(!unit.IsLoggingOut());
}

TEST(UnitVisibilitySession_ACreatureRidesNoTransportIsSeenByNoGroupAndIsSober)
{
    SeenUnit creature;
    SeenUnit const& unit = creature;

    CHECK(unit.GetTransport() == NULL);
    CHECK(!unit.IsGroupVisibleFor(NULL));
    CHECK_EQ(unit.GetDrunkValue(), uint16(0));
}
