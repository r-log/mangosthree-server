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

/// The packets a player's own client is told, built from the player's six facts: the bytes of each
/// builder, the callbacks the session installs on a player, which build the packet at each report
/// and send it to the session the player holds at that moment, the security level read from that
/// session at each call, and the assertion on a callback no session installed.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "WorldPacket.h"
#include "session/packets/PlayerPacketSinks.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    // A creature guid, entry 3000 and counter 42: raw 0xF1300BB80000002A, bytes (low first)
    // 2a 00 00 00 b8 0b 30 f1, so its packed form skips three zero bytes and keeps the high byte.
    const ObjectGuid kCreature(HIGHGUID_UNIT, uint32(3000), uint32(42));
    // A pet guid, entry 416 and counter 1: raw 0xF14001A000000001.
    const ObjectGuid kPet(HIGHGUID_PET, uint32(416), uint32(1));
    // A player guid, counter 15728640: raw 0x0000000000F00000.
    const ObjectGuid kPlayer(HIGHGUID_PLAYER, uint32(15728640));

    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    template <class Fact>
    std::string Built(void (*build)(WorldPacket&, Fact const&), Fact const& fact)
    {
        WorldPacket packet;
        build(packet, fact);
        return Hex(packet);
    }

    AutoRepeatCancelledFact AutoRepeat(ObjectGuid target)
    {
        AutoRepeatCancelledFact fact;
        fact.target = target;
        return fact;
    }

    CurrentPetFact PetIs(ObjectGuid pet)
    {
        CurrentPetFact fact;
        fact.pet = pet;
        return fact;
    }

    StandStateFact Stand(uint8 state)
    {
        StandStateFact fact;
        fact.state = state;
        return fact;
    }

    /// Stands in for the session: records "<opcode>:<hex>" per packet it is sent, and answers its
    /// account security level.
    struct FakeSession
    {
        std::vector<std::string> sent;
        AccountTypes security = SEC_PLAYER;

        void SendPacket(WorldPacket const* packet)
        {
            sent.push_back(Hex(*packet));
        }

        AccountTypes GetSecurity() const
        {
            return security;
        }
    };

    /// Stands in for the player: GetSession() answers whichever session it holds now.
    struct FakeOwner
    {
        FakeSession* session = NULL;

        FakeSession* GetSession() const
        {
            return session;
        }
    };

    /// Reports one of each fact through `callbacks`, in the order of the golden list below.
    void ReportEach(Player::ClientCallbacks const& callbacks)
    {
        ReportClientFact(callbacks.swingOutOfReach, SwingOutOfReachFact());
        ReportClientFact(callbacks.swingBadFacing, SwingBadFacingFact());
        ReportClientFact(callbacks.combatCancelled, CombatCancelledFact());
        ReportClientFact(callbacks.autoRepeatCancelled, AutoRepeat(kCreature));
        ReportClientFact(callbacks.currentPet, PetIs(kPet));
        ReportClientFact(callbacks.standState, Stand(8));
    }

    const char* const kEach[] =
    {
        "0b36:",
        "6c07:",
        "4f04:",
        "6436:f12ab80b30f1",
        "2d26:0100000001000000a00140f1",
        "6f04:08",
    };

    /// The empty-callback case ends the process on its assertion, so it runs only when
    /// MANGOS_TESTS_ABORT_CASE names it (CheckAbortCase.cmake); any other run returns at once.
    bool AbortCaseSelected(char const* name)
    {
        char const* selected = std::getenv("MANGOS_TESTS_ABORT_CASE");
        if (!selected || std::strcmp(selected, name) != 0)
        {
            return false;
        }
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
        return true;
    }
}

// SMSG_ATTACKSWING_NOTINRANGE (0x0B36), SMSG_ATTACKSWING_BADFACING (0x6C07) and SMSG_CANCEL_COMBAT
// (0x4F04): no payload.
TEST(PlayerClientPackets_EmptyNoticesBytes)
{
    CHECK_STR(Built(&BuildAttackSwingNotInRangePacket, SwingOutOfReachFact()), "0b36:");
    CHECK_STR(Built(&BuildAttackSwingBadFacingPacket, SwingBadFacingFact()), "6c07:");
    CHECK_STR(Built(&BuildCancelCombatPacket, CombatCancelledFact()), "4f04:");
}

// SMSG_CANCEL_AUTO_REPEAT (0x6436): the target's packed guid, a mask byte with one bit per non-zero
// byte of the raw guid, then those bytes, low first.
TEST(PlayerClientPackets_CancelAutoRepeatBytes)
{
    REQUIRE(kCreature.GetRawValue() == UI64LIT(0xF1300BB80000002A));
    REQUIRE(kPlayer.GetRawValue() == UI64LIT(0x0000000000F00000));

    // Bytes 0, 4, 5, 6 and 7 are non-zero: mask 11110001 = f1, then 2a b8 0b 30 f1.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(kCreature)), "6436:f12ab80b30f1");
    // Byte 2 only: mask 00000100 = 04, then f0.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(kPlayer)), "6436:04f0");
    // The empty guid: mask 00, no byte.
    CHECK_STR(Built(&BuildCancelAutoRepeatPacket, AutoRepeat(ObjectGuid())), "6436:00");
}

// The packed guid the builder writes is the one an object caches for its own guid (Object::_Create
// sets it with PackedGuid::Set from the guid it stores).
TEST(PlayerClientPackets_CancelAutoRepeatWritesTheCachedPackedGuid)
{
    const ObjectGuid guids[] = { kCreature, kPet, kPlayer, ObjectGuid() };
    for (ObjectGuid const& guid : guids)
    {
        PackedGuid cached;
        cached.Set(guid);
        ByteBuffer expected;
        expected << cached;

        WorldPacket packet;
        BuildCancelAutoRepeatPacket(packet, AutoRepeat(guid));
        REQUIRE(packet.size() == expected.size());
        CHECK(std::memcmp(packet.contents(), expected.contents(), expected.size()) == 0);
    }
}

// SMSG_PET_GUIDS (0x2D26): uint32 1, then the pet's raw guid as a uint64.
TEST(PlayerClientPackets_PetGuidsBytes)
{
    REQUIRE(kPet.GetRawValue() == UI64LIT(0xF14001A000000001));

    // 01000000; the guid, low byte first: 01 00 00 00 a0 01 40 f1.
    CHECK_STR(Built(&BuildPetGuidsPacket, PetIs(kPet)), "2d26:0100000001000000a00140f1");
    CHECK_STR(Built(&BuildPetGuidsPacket, PetIs(kCreature)), "2d26:010000002a000000b80b30f1");
}

// SMSG_STANDSTATE_UPDATE (0x6F04): the state, one byte.
TEST(PlayerClientPackets_StandStateUpdateBytes)
{
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(0)), "6f04:00");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(1)), "6f04:01");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(8)), "6f04:08");
    CHECK_STR(Built(&BuildStandStateUpdatePacket, Stand(255)), "6f04:ff");
}

// The callbacks the session installs turn each fact into the packet above and send it to the
// owner's session, each fact to its own packet.
TEST(PlayerClientPackets_InstalledCallbacksSendTheGoldenPackets)
{
    FakeSession session;
    FakeOwner owner;
    owner.session = &session;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    ReportEach(callbacks);

    REQUIRE(session.sent.size() == 6u);
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK_STR(session.sent[i], kEach[i]);
    }
}

// The session is read at each send, never kept: callbacks made before the owner's session changed
// send to the new one.
TEST(PlayerClientPackets_InstalledCallbacksReadTheSessionAtEachSend)
{
    FakeSession first;
    FakeSession second;
    FakeOwner owner;
    owner.session = &first;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    ReportEach(callbacks);
    owner.session = &second;
    ReportEach(callbacks);

    REQUIRE(first.sent.size() == 6u);
    REQUIRE(second.sent.size() == 6u);
    for (size_t i = 0; i < 6; ++i)
    {
        CHECK_STR(first.sent[i], kEach[i]);
        CHECK_STR(second.sent[i], kEach[i]);
    }
}

// The security level is the session's, read at each call: a change of the session's level, or of
// the session, between two calls changes the answer.
TEST(PlayerClientPackets_SecurityLevelReadsTheSessionAtEachCall)
{
    FakeSession first;
    FakeSession second;
    second.security = SEC_ADMINISTRATOR;
    FakeOwner owner;
    owner.session = &first;

    Player::ClientCallbacks callbacks = ClientCallbacksToSession<Player::ClientCallbacks>(&owner);
    REQUIRE(callbacks.securityLevel);

    CHECK(callbacks.securityLevel() == uint32(SEC_PLAYER));
    first.security = SEC_GAMEMASTER;
    CHECK(callbacks.securityLevel() == uint32(SEC_GAMEMASTER));
    first.security = SEC_CONSOLE;
    CHECK(callbacks.securityLevel() == uint32(SEC_CONSOLE));
    owner.session = &second;
    CHECK(callbacks.securityLevel() == uint32(SEC_ADMINISTRATOR));
    CHECK(first.sent.empty());
    CHECK(second.sent.empty());
}

// A reported fact reaches the callback once, as given.
TEST(PlayerClientPackets_ReportCallsTheCallbackOnce)
{
    std::vector<uint32> seen;
    StandStateSink sink = [&seen](StandStateFact const& fact)
    {
        seen.push_back(fact.state);
    };

    ReportClientFact(sink, Stand(3));
    ReportClientFact(sink, Stand(0));

    REQUIRE(seen.size() == 2u);
    CHECK(seen[0] == 3u);
    CHECK(seen[1] == 0u);
}

TEST(PlayerClientPackets_EmptyCallbackAsserts)
{
    if (!AbortCaseSelected("PlayerClientPackets_EmptyCallbackAsserts"))
    {
        return;
    }
    ReportClientFact(CombatCancelledSink(), CombatCancelledFact());
    testing::ReportFailure(__FILE__, __LINE__, "the empty client callback passed the assertion");
}
