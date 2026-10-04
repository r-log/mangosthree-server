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

/// The packets Unit asks a player to send to its own session: the attack cancel, the auto-repeat
/// cancel, the pet guids, the stand state and a changed melee swing error.
///
/// Unit declares the five protected, with no default argument. A Unit that is not a Player has no
/// session: a Creature probe with its update fields allocated (no map, no AI, no auras, no
/// session) makes them public with using-declarations; each leaves the probe with no aura holder
/// and every update field as it was, for every byte the two byte-taking ones accept and for a NULL
/// and a self target of the auto-repeat cancel. A Player cannot be built in this binary (it needs
/// a WorldSession and a map), so Player's bodies are not run here. Player's attack cancel,
/// auto-repeat cancel and pet guids stay public; its stand state and swing error overrides are
/// private. The static_asserts pin Unit's declarations through the probe, Player's own
/// declarations with Unit's exact signatures (the private ones named in an explicit
/// instantiation, where access is not checked), that a call through a Unit does not compile, and
/// that a call through a Player compiles for the three public ones and not for the two private.

#include "TestHarness.h"
#include "Creature.h"
#include "Player.h"
#include "Unit.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the own-session packets Unit asks a player for
    /// are public here.
    class OwnSessionPacketsUnit : public Creature
    {
        public:
            using Unit::SendAttackSwingCancelAttack;
            using Unit::SendAutoRepeatCancel;
            using Unit::SendPetGUIDs;
            using Unit::SendStandStateUpdate;
            using Unit::ReportSwingError;

            static_assert(std::is_same<decltype(&OwnSessionPacketsUnit::SendAttackSwingCancelAttack),
                                       void (Unit::*)()>::value,
                          "Unit declares the attack cancel");
            static_assert(std::is_same<decltype(&OwnSessionPacketsUnit::SendAutoRepeatCancel),
                                       void (Unit::*)(Unit*)>::value,
                          "Unit declares the auto-repeat cancel");
            static_assert(std::is_same<decltype(&OwnSessionPacketsUnit::SendPetGUIDs), void (Unit::*)()>::value,
                          "Unit declares the pet guids");
            static_assert(std::is_same<decltype(&OwnSessionPacketsUnit::SendStandStateUpdate),
                                       void (Unit::*)(uint8)>::value,
                          "Unit declares the stand state");
            static_assert(std::is_same<decltype(&OwnSessionPacketsUnit::ReportSwingError),
                                       void (Unit::*)(uint8)>::value,
                          "Unit declares the swing error report");

            OwnSessionPacketsUnit() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~OwnSessionPacketsUnit()
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

    static_assert(std::is_same<decltype(&Player::SendAttackSwingCancelAttack), void (Player::*)()>::value,
                  "Player declares the attack cancel with Unit's signature");
    static_assert(std::is_same<decltype(&Player::SendAutoRepeatCancel), void (Player::*)(Unit*)>::value,
                  "Player declares the auto-repeat cancel with Unit's signature");
    static_assert(std::is_same<decltype(&Player::SendPetGUIDs), void (Player::*)()>::value,
                  "Player declares the pet guids with Unit's signature");

    /// Checks the type of the member it is instantiated with. Player's stand state and swing error
    /// overrides are private, so they are named only in the explicit instantiations below: a member
    /// Player does not declare itself has Unit's member pointer type, which fails the check, and an
    /// overloaded name has no type.
    template<class Member, Member member>
    struct PlayerByteOverrideDeclaration
    {
        static_assert(std::is_same<Member, void (Player::*)(uint8)>::value,
                      "Player declares the override with Unit's signature");
    };

    template struct PlayerByteOverrideDeclaration<decltype(&Player::SendStandStateUpdate),
                                                  &Player::SendStandStateUpdate>;
    template struct PlayerByteOverrideDeclaration<decltype(&Player::ReportSwingError), &Player::ReportSwingError>;

    template<class T>
    auto CallsAttackCancel(int) -> decltype(std::declval<T&>().SendAttackSwingCancelAttack(), std::true_type());
    template<class T>
    std::false_type CallsAttackCancel(...);

    template<class T>
    auto CallsAutoRepeatCancel(int)
        -> decltype(std::declval<T&>().SendAutoRepeatCancel(static_cast<Unit*>(NULL)), std::true_type());
    template<class T>
    std::false_type CallsAutoRepeatCancel(...);

    template<class T>
    auto CallsPetGuids(int) -> decltype(std::declval<T&>().SendPetGUIDs(), std::true_type());
    template<class T>
    std::false_type CallsPetGuids(...);

    template<class T>
    auto CallsStandState(int) -> decltype(std::declval<T&>().SendStandStateUpdate(uint8(0)), std::true_type());
    template<class T>
    std::false_type CallsStandState(...);

    template<class T>
    auto CallsSwingError(int) -> decltype(std::declval<T&>().ReportSwingError(uint8(0)), std::true_type());
    template<class T>
    std::false_type CallsSwingError(...);
}

static_assert(decltype(CallsAttackCancel<OwnSessionPacketsUnit>(0))::value &&
              decltype(CallsAutoRepeatCancel<OwnSessionPacketsUnit>(0))::value &&
              decltype(CallsPetGuids<OwnSessionPacketsUnit>(0))::value &&
              decltype(CallsStandState<OwnSessionPacketsUnit>(0))::value &&
              decltype(CallsSwingError<OwnSessionPacketsUnit>(0))::value,
              "a call through the probe reaches each of Unit's five");
static_assert(!decltype(CallsAttackCancel<Unit>(0))::value, "Unit's attack cancel is protected");
static_assert(!decltype(CallsAutoRepeatCancel<Unit>(0))::value, "Unit's auto-repeat cancel is protected");
static_assert(!decltype(CallsPetGuids<Unit>(0))::value, "Unit's pet guids are protected");
static_assert(!decltype(CallsStandState<Unit>(0))::value, "Unit's stand state is protected");
static_assert(!decltype(CallsSwingError<Unit>(0))::value, "Unit's swing error report is protected");
static_assert(decltype(CallsAttackCancel<Player>(0))::value, "Player's attack cancel is public");
static_assert(decltype(CallsAutoRepeatCancel<Player>(0))::value, "Player's auto-repeat cancel is public");
static_assert(decltype(CallsPetGuids<Player>(0))::value, "Player's pet guids are public");
static_assert(!decltype(CallsStandState<Player>(0))::value,
              "Player's stand state is private: a call through a Player does not compile");
static_assert(!decltype(CallsSwingError<Player>(0))::value,
              "Player's swing error report is private: a call through a Player does not compile");

TEST(UnitOwnSessionPackets_ACreatureSendsNothing)
{
    OwnSessionPacketsUnit creature;
    OwnSessionPacketsUnit& unit = creature;
    std::vector<uint32> const fields = creature.Fields();
    CHECK(!fields.empty());

    unit.SendAttackSwingCancelAttack();
    CHECK(unit.GetSpellAuraHolderMap().empty());
    CHECK(creature.Fields() == fields);

    unit.SendAutoRepeatCancel(NULL);
    CHECK(unit.GetSpellAuraHolderMap().empty());
    CHECK(creature.Fields() == fields);

    unit.SendAutoRepeatCancel(&creature);
    CHECK(unit.GetSpellAuraHolderMap().empty());
    CHECK(creature.Fields() == fields);

    unit.SendPetGUIDs();
    CHECK(unit.GetSpellAuraHolderMap().empty());
    CHECK(creature.Fields() == fields);

    for (uint32 value = 0; value <= 0xFF; ++value)
    {
        unit.SendStandStateUpdate(uint8(value));
        CHECK(unit.GetSpellAuraHolderMap().empty());
        CHECK(creature.Fields() == fields);

        unit.ReportSwingError(uint8(value));
        CHECK(unit.GetSpellAuraHolderMap().empty());
        CHECK(creature.Fields() == fields);
    }
}
