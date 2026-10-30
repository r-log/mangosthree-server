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

/// The position write and the moving test: Update's pending commit, the end of a spline, a
/// creature's short teleport and a possessed creature's own movement move a unit through
/// SetPosition, and the auto-repeat spell update asks a player isMoving.
///
/// Unit declares the position write public and pure, and the moving test protected. Creature
/// overrides the write, relocating itself through its map; Player overrides both, public. The
/// teleport argument defaults to false on all three. A Creature probe with its update fields
/// allocated (no map, no AI, no auras) makes Unit's moving test public with a using-declaration;
/// the write needs a map, so it is not run here, and neither are Player's bodies. Unit's isMoving
/// answers false whatever the probe's own movement flags (Unit's member) hold. The static_asserts
/// pin Creature's and Player's declarations (each an override, so with Unit's exact signature),
/// that a call through a Unit, a Creature or a Player compiles, and that the moving test through a
/// Unit does not. A call with four arguments binds false, which an override recording its
/// argument shows.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>

namespace
{
    /// A Creature with its update fields allocated; Unit's moving test is public here.
    class PositionAndMovingUnit : public Creature
    {
        public:
            using Unit::isMoving;

            static_assert(std::is_same<decltype(&Creature::SetPosition),
                                       bool (Creature::*)(float, float, float, float, bool)>::value,
                          "Creature declares the position write");
            static_assert(std::is_same<decltype(&PositionAndMovingUnit::isMoving), bool (Unit::*)() const>::value,
                          "Unit declares the moving test, const");

            PositionAndMovingUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~PositionAndMovingUnit()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    /// Records the teleport argument a call through the probe passes to an override, so a call with
    /// four arguments shows the default it bound.
    class TeleportRecordingUnit : public PositionAndMovingUnit
    {
        public:
            bool SetPosition(float /*x*/, float /*y*/, float /*z*/, float /*orientation*/, bool teleport) override
            {
                ++calls;
                lastTeleport = teleport;
                return true;
            }

            int calls = 0;
            bool lastTeleport = true;
    };

    static_assert(std::is_same<decltype(&Player::SetPosition),
                               bool (Player::*)(float, float, float, float, bool)>::value,
                  "Player declares the position write with Unit's signature");
    static_assert(std::is_same<decltype(&Player::isMoving), bool (Player::*)() const>::value,
                  "Player declares the moving test with Unit's signature, const");

    template<class T>
    auto WritesPosition(int)
        -> decltype(std::declval<T&>().SetPosition(0.0f, 0.0f, 0.0f, 0.0f, false), std::true_type());
    template<class T>
    std::false_type WritesPosition(...);

    template<class T>
    auto WritesPositionWithItsDefault(int)
        -> decltype(std::declval<T&>().SetPosition(0.0f, 0.0f, 0.0f, 0.0f), std::true_type());
    template<class T>
    std::false_type WritesPositionWithItsDefault(...);

    template<class T>
    auto AsksMoving(int) -> decltype(std::declval<T const&>().isMoving(), std::true_type());
    template<class T>
    std::false_type AsksMoving(...);
}

static_assert(decltype(WritesPosition<PositionAndMovingUnit>(0))::value &&
              decltype(WritesPositionWithItsDefault<PositionAndMovingUnit>(0))::value &&
              decltype(AsksMoving<PositionAndMovingUnit>(0))::value,
              "a call through the probe reaches the position write, with and without the teleport"
              " argument, and Unit's moving test");
static_assert(decltype(WritesPosition<Unit>(0))::value &&
              decltype(WritesPositionWithItsDefault<Unit>(0))::value,
              "Unit's position write is public, with and without the teleport argument");
static_assert(decltype(WritesPosition<Creature>(0))::value &&
              decltype(WritesPositionWithItsDefault<Creature>(0))::value,
              "Creature's position write is public, with and without the teleport argument");
static_assert(!decltype(AsksMoving<Unit>(0))::value,
              "Unit's moving test is protected: a call through a Unit does not compile");
static_assert(decltype(WritesPosition<Player>(0))::value &&
              decltype(WritesPositionWithItsDefault<Player>(0))::value,
              "Player's position write is public, with and without the teleport argument");
static_assert(decltype(AsksMoving<Player>(0))::value, "Player's moving test is public");

TEST(UnitPositionAndMoving_ACallWithFourArgumentsBindsNoTeleport)
{
    TeleportRecordingUnit recorder;
    PositionAndMovingUnit& unit = recorder;

    CHECK(unit.SetPosition(1.0f, 2.0f, 3.0f, 0.5f));
    CHECK_EQ(recorder.calls, 1);
    CHECK(!recorder.lastTeleport);

    CHECK(unit.SetPosition(1.0f, 2.0f, 3.0f, 0.5f, true));
    CHECK_EQ(recorder.calls, 2);
    CHECK(recorder.lastTeleport);
}

TEST(UnitPositionAndMoving_ACreatureIsNeverMoving)
{
    PositionAndMovingUnit creature;
    PositionAndMovingUnit const& unit = creature;

    CHECK(!unit.isMoving());

    for (uint32 bit = 0; bit < 32; ++bit)
    {
        creature.ResetClientWord(MovementFlags(uint32(1) << bit));
        CHECK(!unit.isMoving());
    }

    creature.ResetClientWord(movementFlagsMask);
    CHECK(creature.ClientStatus().HasMovementFlag(movementFlagsMask));
    CHECK(!unit.isMoving());

    creature.ResetClientWord(MovementFlags(0xFFFFFFFF));
    CHECK(!unit.isMoving());
}