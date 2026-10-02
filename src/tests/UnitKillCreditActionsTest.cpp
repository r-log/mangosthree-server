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

/// The kill credit and the honor-or-experience test Unit asks a player for: JustKilledCreature
/// credits a player's kill of a critter to its quests, and the kill procs and Improved Blood
/// Presence require a victim whose kill grants honor or experience.
///
/// Unit declares both protected, with no default arguments. A Unit that is not a Player credits
/// nothing and grants nothing: a Creature probe with its update fields allocated (no map, no AI,
/// no auras) makes the two public with using-declarations and is asked through its own reference;
/// a creature holds no quest state, so the kill credit is pinned by compiling through the probe
/// and leaving every update field as it was, and the test answers false for every victim. A Player
/// cannot be built in this binary (it needs a WorldSession and a map); the static_asserts pin
/// Unit's declarations through the probe, that Player declares both public with Unit's exact
/// signature, the test `const` on both sides, and which references a call compiles through. That
/// Player's test is `final` is pinned by the cast proof's text of Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated and a level set; the kill credit and the
    /// honor-or-experience test are public here.
    class KillUnit : public Creature
    {
        public:
            using Unit::KilledMonster;
            using Unit::isHonorOrXPTarget;

            static_assert(std::is_same<decltype(&KillUnit::KilledMonster),
                                       void (Unit::*)(CreatureInfo const*, ObjectGuid)>::value,
                          "Unit declares the kill credit for a creature");
            static_assert(std::is_same<decltype(&KillUnit::isHonorOrXPTarget), bool (Unit::*)(Unit*) const>::value,
                          "Unit declares the honor-or-experience test, const");

            explicit KillUnit(uint32 level) : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
                SetLevel(level);
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~KillUnit()
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

    /// A creature template with an entry and every kill credit entry set.
    CreatureInfo CritterInfo()
    {
        CreatureInfo info = CreatureInfo();
        info.Entry = 721;
        info.CreatureType = CREATURE_TYPE_CRITTER;
        for (int i = 0; i < MAX_KILL_CREDIT; ++i)
        {
            info.KillCredit[i] = 900 + i;
        }
        return info;
    }

    template<class T>
    auto CallsKilledMonster(int)
        -> decltype(std::declval<T&>().KilledMonster(std::declval<CreatureInfo const*>(), ObjectGuid()),
                    std::true_type());
    template<class T>
    std::false_type CallsKilledMonster(...);

    template<class T>
    auto CallsHonorOrXPTarget(int)
        -> decltype(std::declval<T const&>().isHonorOrXPTarget(std::declval<Unit*>()), std::true_type());
    template<class T>
    std::false_type CallsHonorOrXPTarget(...);
}

static_assert(std::is_same<decltype(&Player::KilledMonster), void (Player::*)(CreatureInfo const*, ObjectGuid)>::value,
              "Player credits the creature's entry and its kill credit entries");
static_assert(std::is_same<decltype(&Player::isHonorOrXPTarget), bool (Player::*)(Unit*) const>::value,
              "Player answers by its level and the victim's kind, const");

static_assert(!decltype(CallsKilledMonster<Unit>(0))::value,
              "Unit's kill credit is protected: a call through a Unit does not compile");
static_assert(decltype(CallsKilledMonster<Player>(0))::value,
              "Player's kill credit is public: a call through a Player compiles");
static_assert(!decltype(CallsHonorOrXPTarget<Unit>(0))::value,
              "Unit's honor-or-experience test is protected: a call through a Unit does not compile");
static_assert(decltype(CallsHonorOrXPTarget<Player>(0))::value,
              "Player's honor-or-experience test is public: a call through a const Player compiles");

TEST(UnitKillCreditActions_ACreatureIsCreditedNothingForAKill)
{
    KillUnit creature(80);
    KillUnit& unit = creature;
    CreatureInfo const info = CritterInfo();
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());
    CHECK_EQ(creature.getLevel(), uint32(80));

    unit.KilledMonster(&info, ObjectGuid(HIGHGUID_UNIT, uint32(721), uint32(4711)));
    CHECK(creature.Fields() == fields);
    unit.KilledMonster(&info, ObjectGuid());
    CHECK(creature.Fields() == fields);
    unit.KilledMonster(&info, ObjectGuid(HIGHGUID_UNIT, uint32(721), uint32(4712)));
    CHECK(creature.Fields() == fields);

    CHECK_EQ(info.Entry, uint32(721));
    CHECK_EQ(info.KillCredit[0], uint32(900));
}

TEST(UnitKillCreditActions_ACreatureFindsNoVictimWorthHonorOrExperience)
{
    KillUnit creature(80);
    KillUnit const& unit = creature;
    KillUnit grey(1);
    KillUnit equal(80);
    KillUnit higher(85);
    std::vector<uint32> const fields = creature.Fields();

    CHECK(!unit.isHonorOrXPTarget(&grey));
    CHECK(!unit.isHonorOrXPTarget(&equal));
    CHECK(!unit.isHonorOrXPTarget(&higher));
    CHECK(!unit.isHonorOrXPTarget(&creature));
    CHECK(!unit.isHonorOrXPTarget(NULL));

    CHECK(creature.Fields() == fields);
}
