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

/// The short teleport on a unit's own map, which Unit asks of a player: NearTeleportTo calls
/// TeleportNear on a player, keeping its transport, combat and pet, and marking the teleport as a
/// spell's when the unit's own cast moves it.
///
/// Unit declares it protected and Player's override private, so only NearTeleportTo reaches it. A
/// Unit that is not a Player is relocated in place by NearTeleportTo's other arm and never reaches
/// it there: a Creature probe with its update fields allocated (no map, no AI, no auras) makes Unit's
/// default public with a using-declaration. Unit's TeleportNear leaves the probe as it was:
/// placement, update fields, movement flags and auras, for several destinations and both casting
/// values. A Player cannot be built in this binary (it needs a WorldSession and a map), so Player's
/// body is not run here. The static_asserts pin Unit's declaration through the probe and Player's
/// own declaration with Unit's exact signature (through an explicit instantiation, since it is
/// private), and that a call through a Unit, a const probe or a Player does not compile.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the short teleport Unit asks of a player is
    /// public here.
    class NearTeleportUnit : public Creature
    {
        public:
            using Unit::TeleportNear;

            static_assert(std::is_same<decltype(&NearTeleportUnit::TeleportNear),
                                       void (Unit::*)(float, float, float, float, bool)>::value,
                          "Unit declares the short teleport, non-const");

            NearTeleportUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~NearTeleportUnit()
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

    /// What a call may not change on a unit: its placement, update fields, movement flags and auras.
    struct Snapshot
    {
        explicit Snapshot(NearTeleportUnit const& unit)
            : x(unit.Where().X()), y(unit.Where().Y()), z(unit.Where().Z()), facing(unit.Where().Facing()),
              fields(unit.Fields()), flags(unit.m_movementInfo.GetMovementFlags()),
              flags2(unit.m_movementInfo.GetMovementFlags2()), auras(unit.GetSpellAuraHolderMap().size())
        {
        }

        bool operator==(Snapshot const& other) const
        {
            return x == other.x && y == other.y && z == other.z && facing == other.facing &&
                   fields == other.fields && flags == other.flags && flags2 == other.flags2 &&
                   auras == other.auras;
        }

        float x, y, z, facing;
        std::vector<uint32> fields;
        MovementFlags flags;
        MovementFlags2 flags2;
        size_t auras;
    };

    /// Checks the type of the member it is instantiated with. Player's short teleport is private,
    /// so it is named only in the explicit instantiation below: a member Player does not declare
    /// itself has Unit's member pointer type, which fails the check, and an overloaded name has no
    /// type.
    template<class Member, Member member>
    struct PlayerNearTeleportDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)(float, float, float, float, bool)>::value,
                      "Player declares the short teleport with Unit's signature");
    };

    template struct PlayerNearTeleportDeclaration<decltype(&Player::TeleportNear), &Player::TeleportNear>;

    template<class T>
    auto TeleportsNear(int)
        -> decltype(std::declval<T&>().TeleportNear(0.0f, 0.0f, 0.0f, 0.0f, false), std::true_type());
    template<class T>
    std::false_type TeleportsNear(...);

    template<class T>
    auto TeleportsNearThroughConst(int)
        -> decltype(std::declval<T const&>().TeleportNear(0.0f, 0.0f, 0.0f, 0.0f, false), std::true_type());
    template<class T>
    std::false_type TeleportsNearThroughConst(...);

    /// The destinations asked for: none is the probe's own placement.
    struct Destination
    {
        float x, y, z, orientation;
    };

    Destination const destinations[] =
    {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 15.5f, -20.25f, 31.0f, 3.0f },
        { -8318.19f, -993.662f, 176.956f, 5.65024f },
        { 10.0f, 20.0f, 30.0f, 6.2f },
    };
}

static_assert(decltype(TeleportsNear<NearTeleportUnit>(0))::value,
              "a call through the probe reaches Unit's short teleport");
static_assert(!decltype(TeleportsNearThroughConst<NearTeleportUnit>(0))::value,
              "Unit's short teleport is not const: a call through a const unit does not compile");
static_assert(!decltype(TeleportsNear<Unit>(0))::value,
              "Unit's short teleport is protected: a call through a Unit does not compile");
static_assert(!decltype(TeleportsNear<Player>(0))::value,
              "Player's short teleport is private: a call through a Player does not compile");

TEST(UnitNearTeleport_ACreatureIsNotMovedByTheDefault)
{
    NearTeleportUnit creature;
    NearTeleportUnit& unit = creature;
    creature.Place().MoveTo(10.0f, 20.0f, 30.0f, 1.0f);
    Snapshot const before(creature);
    CHECK(!before.fields.empty());

    for (Destination const& to : destinations)
    {
        unit.TeleportNear(to.x, to.y, to.z, to.orientation, false);
        CHECK(Snapshot(creature) == before);
        unit.TeleportNear(to.x, to.y, to.z, to.orientation, true);
        CHECK(Snapshot(creature) == before);
    }
}
