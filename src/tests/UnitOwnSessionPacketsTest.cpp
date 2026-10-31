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

/// The packets Unit tells a player's own client: the attack cancel, the auto-repeat cancel, the
/// pet guids, the stand state and a changed melee swing error.
///
/// Unit defines the five once, with no default argument: the attack cancel and the auto-repeat
/// cancel are public, the other three protected, and Player declares none of them. A unit reports
/// each through the client callbacks a player points it at; a unit that is not a player has none
/// and is told nothing. A Creature probe with its update fields allocated (no map, no AI, no auras,
/// no session) makes the five public with using-declarations; each leaves the probe with no aura
/// holder and every update field as it was, for every byte the two byte-taking ones accept and for
/// a NULL and a self target of the auto-repeat cancel. A player seated on a socketless session,
/// with the session's callbacks installed, sends each packet once, the swing error only when it
/// differs from the last one sent. The static_asserts pin Unit's declarations through the probe,
/// that Player declares none of the five (the protected ones named in an explicit instantiation,
/// where access is not checked), and which calls compile through a Unit and through a Player.

#include "TestHarness.h"
#include "Auth/BigNumber.h"
#include "Creature.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "session/packets/PlayerPacketSinks.h"

#include <type_traits>
#include <utility>
#include <vector>

namespace
{
    /// A Creature with its update fields allocated; the five packets Unit tells a player's client
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

    /// A player with its update fields allocated, seated on its session, in no world; the three
    /// protected packets are public here.
    class OwnClientPlayer : public Player
    {
        public:
            using Unit::SendPetGUIDs;
            using Unit::SendStandStateUpdate;
            using Unit::ReportSwingError;

            explicit OwnClientPlayer(WorldSession& session) : Player(&session)
            {
                Object::_Create(1, 0, HIGHGUID_PLAYER);
                session.SetPlayer(this);
            }

            ~OwnClientPlayer()
            {
                SessionOf(*this)->SetPlayer(nullptr);
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };

    void KeepOpcode(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<uint16>*>(context)->push_back(packet.GetOpcode());
    }

    static_assert(std::is_same<decltype(&Player::SendAttackSwingCancelAttack), void (Unit::*)()>::value,
                  "Player does not declare the attack cancel: its member pointer is Unit's");
    static_assert(std::is_same<decltype(&Player::SendAutoRepeatCancel), void (Unit::*)(Unit*)>::value,
                  "Player does not declare the auto-repeat cancel: its member pointer is Unit's");

    /// Checks the type of the member it is instantiated with. Unit's pet guids, stand state and
    /// swing error report are protected, so they are named only in the explicit instantiations
    /// below: a member Player declared itself would have Player's member pointer type.
    template<class Member, Member member, class Expected>
    struct UnitMemberThroughPlayer
    {
        static_assert(std::is_same<Member, Expected>::value,
                      "Player does not declare it: its member pointer is Unit's");
    };

    template struct UnitMemberThroughPlayer<decltype(&Player::SendPetGUIDs), &Player::SendPetGUIDs, void (Unit::*)()>;
    template struct UnitMemberThroughPlayer<decltype(&Player::SendStandStateUpdate), &Player::SendStandStateUpdate,
                                            void (Unit::*)(uint8)>;
    template struct UnitMemberThroughPlayer<decltype(&Player::ReportSwingError), &Player::ReportSwingError,
                                            void (Unit::*)(uint8)>;

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
static_assert(decltype(CallsAttackCancel<Unit>(0))::value, "Unit's attack cancel is public");
static_assert(decltype(CallsAutoRepeatCancel<Unit>(0))::value, "Unit's auto-repeat cancel is public");
static_assert(!decltype(CallsPetGuids<Unit>(0))::value, "Unit's pet guids are protected");
static_assert(!decltype(CallsStandState<Unit>(0))::value, "Unit's stand state is protected");
static_assert(!decltype(CallsSwingError<Unit>(0))::value, "Unit's swing error report is protected");
static_assert(decltype(CallsAttackCancel<Player>(0))::value, "a call through a Player reaches the attack cancel");
static_assert(decltype(CallsAutoRepeatCancel<Player>(0))::value,
              "a call through a Player reaches the auto-repeat cancel");
static_assert(!decltype(CallsPetGuids<Player>(0))::value,
              "the pet guids stay protected: a call through a Player does not compile");
static_assert(!decltype(CallsStandState<Player>(0))::value,
              "the stand state stays protected: a call through a Player does not compile");
static_assert(!decltype(CallsSwingError<Player>(0))::value,
              "the swing error report stays protected: a call through a Player does not compile");

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

TEST(UnitOwnSessionPackets_APlayerSendsEachPacketToItsOwnSession)
{
    std::vector<uint16> sent;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepOpcode, &sent);
    OwnClientPlayer player(session);
    InstallPlayerPacketSinks(player);

    player.SendAttackSwingCancelAttack();
    player.SendAutoRepeatCancel(&player);
    player.SendPetGUIDs();
    player.SetPetGuid(ObjectGuid(HIGHGUID_PET, uint32(1234), uint32(5)));
    player.SendPetGUIDs();
    player.SendStandStateUpdate(UNIT_STAND_STATE_SIT);

    CHECK(sent == std::vector<uint16>({uint16(SMSG_CANCEL_COMBAT), uint16(SMSG_CANCEL_AUTO_REPEAT), uint16(SMSG_PET_GUIDS),
                                       uint16(SMSG_STANDSTATE_UPDATE)}));
}

TEST(UnitOwnSessionPackets_APlayersSwingErrorIsSentOnlyWhenItChanges)
{
    std::vector<uint16> sent;
    WorldSession session(1, "client", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&KeepOpcode, &sent);
    OwnClientPlayer player(session);
    InstallPlayerPacketSinks(player);

    player.ReportSwingError(1);
    player.ReportSwingError(1);
    player.ReportSwingError(2);
    player.ReportSwingError(0);
    player.ReportSwingError(0);
    player.ReportSwingError(1);

    CHECK(sent == std::vector<uint16>({uint16(SMSG_ATTACKSWING_NOTINRANGE), uint16(SMSG_ATTACKSWING_BADFACING),
                                       uint16(SMSG_ATTACKSWING_NOTINRANGE)}));
}
