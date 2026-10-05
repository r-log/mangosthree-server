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

/// The client's control of a unit's movement, which Unit gives or takes through a player: the fear
/// and confuse states call SetClientControl on a player when the first of them takes hold and when
/// the last one ends.
///
/// Unit declares it protected. A Unit that is not a Player is never given or taken control there:
/// a Creature probe with its update fields allocated (no map, no AI, no auras) makes it public with
/// a using-declaration. Unit's SetClientControl leaves the probe, and a second probe passed as the
/// target, as they were: placement, update fields and movement flags, for allowMove 0 and 1, and
/// for a NULL target. A Player cannot be built in this binary (it needs a WorldSession and a map),
/// so Player's body is not run here; Player's override stays public. The static_asserts pin Unit's
/// declaration through the probe and Player's own declaration with Unit's exact signature, that a
/// call through a Unit does not compile, and that a call through a Player compiles.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the client control Unit gives or takes through
    /// a player is public here.
    class ClientControlUnit : public Creature
    {
        public:
            using Unit::SetClientControl;

            static_assert(std::is_same<decltype(&ClientControlUnit::SetClientControl),
                                       void (Unit::*)(Unit*, uint8)>::value,
                          "Unit declares the client control, non-const");

            ClientControlUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ClientControlUnit()
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
        explicit Snapshot(ClientControlUnit const& unit)
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

    static_assert(std::is_same<decltype(&Player::SetClientControl), void (Player::*)(Unit*, uint8)>::value,
                  "Player declares the client control with Unit's signature");

    template<class T>
    auto ControlsClient(int)
        -> decltype(std::declval<T&>().SetClientControl(std::declval<Unit*>(), uint8(0)), std::true_type());
    template<class T>
    std::false_type ControlsClient(...);

    template<class T>
    auto ControlsClientThroughConst(int)
        -> decltype(std::declval<T const&>().SetClientControl(std::declval<Unit*>(), uint8(0)), std::true_type());
    template<class T>
    std::false_type ControlsClientThroughConst(...);
}

static_assert(decltype(ControlsClient<ClientControlUnit>(0))::value,
              "a call through the probe reaches Unit's client control");
static_assert(!decltype(ControlsClientThroughConst<ClientControlUnit>(0))::value,
              "Unit's client control is not const: a call through a const unit does not compile");
static_assert(!decltype(ControlsClient<Unit>(0))::value,
              "Unit's client control is protected: a call through a Unit does not compile");
static_assert(decltype(ControlsClient<Player>(0))::value, "Player's client control is public");

TEST(UnitClientControl_ACreatureGivesOrTakesNothingOfItsOwn)
{
    ClientControlUnit creature;
    ClientControlUnit& unit = creature;
    creature.Place().MoveTo(10.0f, 20.0f, 30.0f, 1.0f);
    Snapshot const before(creature);
    CHECK(!before.fields.empty());

    for (uint8 allowMove = 0; allowMove < 2; ++allowMove)
    {
        unit.SetClientControl(&creature, allowMove);
        CHECK(Snapshot(creature) == before);
    }
}

TEST(UnitClientControl_ACreatureGivesOrTakesNothingOfAnother)
{
    ClientControlUnit creature;
    ClientControlUnit target;
    ClientControlUnit& unit = creature;
    creature.Place().MoveTo(10.0f, 20.0f, 30.0f, 1.0f);
    target.Place().MoveTo(-5.0f, 6.5f, 7.25f, 2.0f);
    Snapshot const self(creature);
    Snapshot const other(target);

    for (uint8 allowMove = 0; allowMove < 2; ++allowMove)
    {
        unit.SetClientControl(&target, allowMove);
        CHECK(Snapshot(creature) == self);
        CHECK(Snapshot(target) == other);
    }
}

TEST(UnitClientControl_ACreatureIgnoresANullTarget)
{
    ClientControlUnit creature;
    ClientControlUnit& unit = creature;
    creature.Place().MoveTo(10.0f, 20.0f, 30.0f, 1.0f);
    Snapshot const before(creature);

    unit.SetClientControl(NULL, 0);
    CHECK(Snapshot(creature) == before);
    unit.SetClientControl(NULL, 1);
    CHECK(Snapshot(creature) == before);
}
