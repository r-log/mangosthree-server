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

/// What three procs ask of a player: the Aura of Madness proc has the player say a text, the
/// Shattered Sun pendant's proc strikes the player's selection when there is no victim, and Prayer
/// of Mending jumps to a random raid member of the player.
///
/// Unit declares the three protected, with no default arguments. A Unit that is not a Player says
/// nothing, has no selection and has no raid: a Creature probe with its update fields allocated
/// (no map, no AI, no auras, no session) makes the three public with using-declarations and is
/// asked through its own reference; the say leaves every update field as it was, the selection is
/// an empty guid whatever target the creature holds, and the raid member is NULL. A Player cannot
/// be built in this binary (it needs a WorldSession and a map); the static_asserts pin Unit's
/// declarations through the probe, that Player declares the three public with Unit's exact
/// signature, the selection returned by value and `const` on both sides, and which references a
/// call compiles through. That Player's selection and raid member are `final` is pinned by the
/// cast proof's text of Player.h.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the say, the selection and the raid member are
    /// public here.
    class ProcOneOffUnit : public Creature
    {
        public:
            using Unit::Say;
            using Unit::GetSelectionGuid;
            using Unit::GetNextRandomRaidMember;

            static_assert(std::is_same<decltype(&ProcOneOffUnit::Say),
                                       void (Unit::*)(std::string const&, uint32)>::value,
                          "Unit declares the say");
            static_assert(std::is_same<decltype(&ProcOneOffUnit::GetSelectionGuid),
                                       ObjectGuid (Unit::*)() const>::value,
                          "Unit declares the selection, by value, const");
            static_assert(std::is_same<decltype(&ProcOneOffUnit::GetNextRandomRaidMember),
                                       Player* (Unit::*)(float)>::value,
                          "Unit declares the raid member");

            ProcOneOffUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~ProcOneOffUnit()
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
    auto CallsSay(int) -> decltype(std::declval<T&>().Say(std::string(), uint32(LANG_UNIVERSAL)), std::true_type());
    template<class T>
    std::false_type CallsSay(...);

    template<class T>
    auto CallsGetSelectionGuid(int) -> decltype(std::declval<T const&>().GetSelectionGuid(), std::true_type());
    template<class T>
    std::false_type CallsGetSelectionGuid(...);

    template<class T>
    auto CallsGetNextRandomRaidMember(int) -> decltype(std::declval<T&>().GetNextRandomRaidMember(1.0f), std::true_type());
    template<class T>
    std::false_type CallsGetNextRandomRaidMember(...);
}

static_assert(std::is_same<decltype(&Player::Say), void (Player::*)(std::string const&, uint32)>::value,
              "Player sends the say message");
static_assert(std::is_same<decltype(&Player::GetSelectionGuid), ObjectGuid (Player::*)() const>::value,
              "Player returns its selection, by value, const");
static_assert(std::is_same<decltype(std::declval<Player const&>().GetSelectionGuid()), ObjectGuid>::value,
              "a call of Player's selection yields an ObjectGuid value, not a reference");
static_assert(std::is_same<decltype(&Player::GetNextRandomRaidMember), Player* (Player::*)(float)>::value,
              "Player returns a random member of its group");

static_assert(!decltype(CallsSay<Unit>(0))::value,
              "Unit's say is protected: a call through a Unit does not compile");
static_assert(decltype(CallsSay<Player>(0))::value,
              "Player's say is public: a call through a Player compiles");
static_assert(!decltype(CallsGetSelectionGuid<Unit>(0))::value,
              "Unit's selection is protected: a call through a Unit does not compile");
static_assert(decltype(CallsGetSelectionGuid<Player>(0))::value,
              "Player's selection is public: a call through a const Player compiles");
static_assert(!decltype(CallsGetNextRandomRaidMember<Unit>(0))::value,
              "Unit's raid member is protected: a call through a Unit does not compile");
static_assert(decltype(CallsGetNextRandomRaidMember<Player>(0))::value,
              "Player's raid member is public: a call through a Player compiles");

TEST(UnitProcOneOffActions_ACreatureSaysNothing)
{
    ProcOneOffUnit creature;
    ProcOneOffUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());

    unit.Say("This is Madness!", LANG_UNIVERSAL);
    CHECK(creature.Fields() == fields);
    unit.Say(std::string(), LANG_COMMON);
    CHECK(creature.Fields() == fields);
}

TEST(UnitProcOneOffActions_ACreatureHasNoSelection)
{
    ProcOneOffUnit creature;
    ProcOneOffUnit const& unit = creature;

    CHECK(unit.GetSelectionGuid().IsEmpty());
    CHECK(unit.GetSelectionGuid() == ObjectGuid());

    ObjectGuid const target(HIGHGUID_UNIT, uint32(1), uint32(1));
    creature.SetTargetGuid(target);
    std::vector<uint32> const fields = creature.Fields();
    CHECK(creature.GetTargetGuid() == target);
    CHECK(unit.GetSelectionGuid().IsEmpty());
    CHECK(creature.Fields() == fields);
}

TEST(UnitProcOneOffActions_ACreatureHasNoRaidMember)
{
    ProcOneOffUnit creature;
    ProcOneOffUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();

    CHECK(unit.GetNextRandomRaidMember(0.0f) == NULL);
    CHECK(unit.GetNextRandomRaidMember(40.0f) == NULL);
    CHECK(unit.GetNextRandomRaidMember(100000.0f) == NULL);

    CHECK(creature.Fields() == fields);
}
