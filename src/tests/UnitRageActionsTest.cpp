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

/// The rage action Unit asks a rage user for: DealDamage awards rage for a main-hand or off-hand
/// weapon hit the unit dealt.
///
/// Unit declares it protected, with no default arguments. A Unit that is not a Player does nothing:
/// a Creature probe with its update fields allocated (no map, no AI, no auras), rage as its power
/// and some rage already held, makes the action public with a using-declaration and is asked
/// through its own reference with the shapes of a hit dealt and a hit taken; every update field,
/// the power ones included, stays as it was. A Player cannot be built in this binary (it needs a
/// WorldSession and a map); the static_asserts pin Unit's declaration through the probe, that
/// Player declares it public with Unit's exact signature, and which references a call compiles
/// through.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated, rage as its power and 25 rage of 100 held; the
    /// rage action is public here.
    class RageUnit : public Creature
    {
        public:
            using Unit::RewardRage;

            static_assert(std::is_same<decltype(&RageUnit::RewardRage), void (Unit::*)(uint32, uint32, bool)>::value,
                          "Unit declares the rage awarded from a hit");

            RageUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                SetPowerType(POWER_RAGE);
                SetMaxPower(POWER_RAGE, 1000);
                SetPower(POWER_RAGE, 250);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~RageUnit()
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
    auto CallsRewardRage(int)
        -> decltype(std::declval<T&>().RewardRage(uint32(100), uint32(35), true), std::true_type());
    template<class T>
    std::false_type CallsRewardRage(...);
}

static_assert(std::is_same<decltype(&Player::RewardRage), void (Player::*)(uint32, uint32, bool)>::value,
              "Player converts a hit's damage into rage");

static_assert(!decltype(CallsRewardRage<Unit>(0))::value,
              "Unit's rage action is protected: a call through a Unit does not compile");
static_assert(decltype(CallsRewardRage<Player>(0))::value,
              "Player's rage action is public: a call through a Player compiles");

TEST(UnitRageActions_ACreatureGainsNoRageFromAHitItDealt)
{
    RageUnit creature;
    RageUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());
    CHECK_EQ(int(creature.GetPowerType()), int(POWER_RAGE));
    CHECK_EQ(creature.GetPower(POWER_RAGE), uint32(250));
    CHECK_EQ(creature.GetMaxPower(POWER_RAGE), uint32(1000));

    unit.RewardRage(100, 35, true);
    unit.RewardRage(2000, 70, true);
    unit.RewardRage(1, 0, true);

    CHECK(creature.Fields() == fields);
    CHECK_EQ(creature.GetPower(POWER_RAGE), uint32(250));
}

TEST(UnitRageActions_ACreatureGainsNoRageFromAHitItTook)
{
    RageUnit creature;
    RageUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();

    unit.RewardRage(100, 0, false);
    unit.RewardRage(5000, 0, false);

    CHECK(creature.Fields() == fields);
    CHECK_EQ(creature.GetPower(POWER_RAGE), uint32(250));
}
