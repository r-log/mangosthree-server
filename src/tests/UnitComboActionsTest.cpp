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

/// The two combo-point actions Unit asks a warrior for: ProcDamageAndSpellFor adds a combo point
/// on a target that dodged, and ClearAllReactives and the end of the Overpower window clear them.
///
/// Unit declares both protected, with no default arguments. A Unit that is not a Player does
/// nothing: a Creature probe with its update fields allocated (no map, no AI, no auras) makes the
/// two and the reactive timers public with using-declarations and is asked through its own
/// reference; the calls leave every update field of the probe and of its target, and every
/// reactive timer, as they were. A Player cannot be built in this binary (it needs a WorldSession
/// and a map); the static_asserts pin Unit's declarations through the probe, that Player declares
/// both public with Unit's exact signature, and which references a call compiles through.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated, a target named and the Overpower window open;
    /// the two combo-point actions and the reactive timers are public here.
    class ComboUnit : public Creature
    {
        public:
            using Unit::AddComboPoints;
            using Unit::ClearComboPoints;
            using Unit::m_reactiveTimer;

            static_assert(std::is_same<decltype(&ComboUnit::AddComboPoints), void (Unit::*)(Unit*, int8)>::value,
                          "Unit declares the combo points added on a target");
            static_assert(std::is_same<decltype(&ComboUnit::ClearComboPoints), void (Unit::*)()>::value,
                          "Unit declares the combo points cleared");

            explicit ComboUnit(uint32 targetCounter) : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                SetTargetGuid(ObjectGuid(HIGHGUID_UNIT, uint32(3), targetCounter));
                StartReactiveTimer(REACTIVE_OVERPOWER);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ComboUnit()
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

            std::vector<uint32> Reactives() const
            {
                return std::vector<uint32>(m_reactiveTimer, m_reactiveTimer + MAX_REACTIVE);
            }
    };

    template<class T>
    auto CallsAddComboPoints(int)
        -> decltype(std::declval<T&>().AddComboPoints(static_cast<Unit*>(NULL), int8(1)), std::true_type());
    template<class T>
    std::false_type CallsAddComboPoints(...);

    template<class T>
    auto CallsClearComboPoints(int) -> decltype(std::declval<T&>().ClearComboPoints(), std::true_type());
    template<class T>
    std::false_type CallsClearComboPoints(...);
}

static_assert(std::is_same<decltype(&Player::AddComboPoints), void (Player::*)(Unit*, int8)>::value,
              "Player adds the combo points on its combo target");
static_assert(std::is_same<decltype(&Player::ClearComboPoints), void (Player::*)()>::value,
              "Player clears its combo points and its combo target");

static_assert(!decltype(CallsAddComboPoints<Unit>(0))::value && !decltype(CallsClearComboPoints<Unit>(0))::value,
              "Unit's two are protected: a call through a Unit does not compile");
static_assert(decltype(CallsAddComboPoints<Player>(0))::value && decltype(CallsClearComboPoints<Player>(0))::value,
              "Player's two are public: a call through a Player compiles");

TEST(UnitComboActions_ACreatureAddsNoComboPoints)
{
    ComboUnit creature(80);
    ComboUnit target(81);
    ComboUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    std::vector<uint32> const targetFields = target.Fields();
    std::vector<uint32> const reactives = creature.Reactives();
    CHECK(!fields.empty());
    CHECK_EQ(reactives[REACTIVE_OVERPOWER], uint32(REACTIVE_TIMER_START));

    unit.AddComboPoints(&target, 1);
    unit.AddComboPoints(&target, 5);
    unit.AddComboPoints(&target, -1);

    CHECK(creature.Fields() == fields);
    CHECK(target.Fields() == targetFields);
    CHECK(creature.Reactives() == reactives);
    CHECK(target.Reactives() == reactives);
}

TEST(UnitComboActions_ACreatureClearsNoComboPoints)
{
    ComboUnit creature(82);
    ComboUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    std::vector<uint32> const reactives = creature.Reactives();

    unit.ClearComboPoints();
    unit.ClearComboPoints();

    CHECK(creature.Fields() == fields);
    CHECK(creature.Reactives() == reactives);
    CHECK(!creature.IsInWorld());
}
