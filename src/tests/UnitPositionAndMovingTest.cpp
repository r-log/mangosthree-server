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

/// The position write and the moving test Unit asks a player for: Update's pending commit and the
/// end of a spline move a player through SetPosition, and the auto-repeat spell update asks a
/// player isMoving.
///
/// Unit declares both protected; SetPosition's teleport argument defaults to false on Unit and on
/// Player, and a call through a Unit binds Unit's. A Unit that is not a Player is relocated by its
/// map at the same call sites and is never asked whether it moves: a Creature probe with its update
/// fields allocated (no map, no AI, no auras) makes both public with using-declarations. Unit's
/// SetPosition returns false and leaves the probe's placement and update fields as they were, with
/// and without the teleport argument; Unit's isMoving answers false whatever the probe's own
/// movement flags (Unit's member) hold. A Player cannot be built in this binary (it needs a
/// WorldSession and a map), so Player's bodies are not run here. Both Player overrides stay public.
/// The static_asserts pin Unit's declarations through the probe and Player's own declarations with
/// Unit's exact signatures, that a call through a Unit does not compile, and that a call through a
/// Player compiles. A call with four arguments through the probe binds Unit's default, which an
/// override recording its argument shows to be false. That Player's isMoving is `final`, and the
/// text of both default arguments, are pinned by the cast proof's text of Unit.h and Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the position write and the moving test Unit
    /// asks a player for are public here.
    class PositionAndMovingUnit : public Creature
    {
        public:
            using Unit::SetPosition;
            using Unit::isMoving;

            static_assert(std::is_same<decltype(&PositionAndMovingUnit::SetPosition),
                                       bool (Unit::*)(float, float, float, float, bool)>::value,
                          "Unit declares the position write");
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

    struct Coordinates
    {
        float x, y, z, orientation;
    };
}

static_assert(decltype(WritesPosition<PositionAndMovingUnit>(0))::value &&
              decltype(WritesPositionWithItsDefault<PositionAndMovingUnit>(0))::value &&
              decltype(AsksMoving<PositionAndMovingUnit>(0))::value,
              "a call through the probe reaches Unit's position write, with and without the teleport "
              "argument, and Unit's moving test");
static_assert(!decltype(WritesPosition<Unit>(0))::value &&
              !decltype(WritesPositionWithItsDefault<Unit>(0))::value,
              "Unit's position write is protected: a call through a Unit does not compile");
static_assert(!decltype(AsksMoving<Unit>(0))::value,
              "Unit's moving test is protected: a call through a Unit does not compile");
static_assert(decltype(WritesPosition<Player>(0))::value &&
              decltype(WritesPositionWithItsDefault<Player>(0))::value,
              "Player's position write is public, with and without the teleport argument");
static_assert(decltype(AsksMoving<Player>(0))::value, "Player's moving test is public");

TEST(UnitPositionAndMoving_ACreatureIsNotMovedByTheWrite)
{
    PositionAndMovingUnit creature;
    PositionAndMovingUnit& unit = creature;
    creature.Place().MoveTo(10.0f, 20.0f, 30.0f, 1.0f);
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());

    Coordinates const sets[] =
    {
        { 10.0f, 20.0f, 30.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { -1234.5f, 678.25f, -9.75f, 3.0f },
        { 1.0e9f, -1.0e9f, 1.0e9f, 6.0f },
    };

    for (Coordinates const& c : sets)
    {
        CHECK(!unit.SetPosition(c.x, c.y, c.z, c.orientation));
        CHECK(!unit.SetPosition(c.x, c.y, c.z, c.orientation, false));
        CHECK(!unit.SetPosition(c.x, c.y, c.z, c.orientation, true));

        CHECK_EQ(creature.Where().X(), 10.0f);
        CHECK_EQ(creature.Where().Y(), 20.0f);
        CHECK_EQ(creature.Where().Z(), 30.0f);
        CHECK_EQ(creature.Where().Facing(), 1.0f);
        CHECK(creature.Fields() == fields);
        CHECK(unit.GetSpellAuraHolderMap().empty());
    }
}

TEST(UnitPositionAndMoving_ACallThroughUnitBindsUnitsDefault)
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
        creature.m_movementInfo.SetMovementFlags(MovementFlags(uint32(1) << bit));
        CHECK(!unit.isMoving());
    }

    creature.m_movementInfo.SetMovementFlags(movementFlagsMask);
    CHECK(creature.m_movementInfo.HasMovementFlag(movementFlagsMask));
    CHECK(!unit.isMoving());

    creature.m_movementInfo.SetMovementFlags(MovementFlags(0xFFFFFFFF));
    CHECK(!unit.isMoving());
}
