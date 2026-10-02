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

/// The session's cooldown packets, built from the cooldown manager's two facts: the bytes of each
/// builder, and the callbacks the session installs on a character, which build the packet at each
/// report and send it to the session the character holds at that moment.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "session/packets/spells/CooldownPackets.h"
#include "session/packets/spells/CooldownPacketSinks.h"

#include <string>
#include <vector>

namespace
{
    // The owner's guid in the packets: raw 0x0100FF0000A5003C, so its bytes (low first) are
    // g0 3c, g1 00, g2 a5, g3 00, g4 00, g5 ff, g6 00, g7 01 -- zero and non-zero bytes on both
    // sides of every mask, and g7 = 01, which the packed form writes as 01 ^ 1 = 00.
    const uint64 kOwnerRaw = UI64LIT(0x0100FF0000A5003C);

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

    CooldownEventFact Event(uint32 spellId, ObjectGuid owner)
    {
        CooldownEventFact fact;
        fact.spellId = spellId;
        fact.owner = owner;
        return fact;
    }

    CooldownsClearedFact Cleared(ObjectGuid owner, std::vector<uint32> const& spellIds)
    {
        CooldownsClearedFact fact;
        fact.owner = owner;
        fact.spellIds = spellIds;
        return fact;
    }

    std::string EventHex(CooldownEventFact const& fact)
    {
        WorldPacket packet;
        BuildCooldownEventPacket(packet, fact);
        return Hex(packet);
    }

    std::string ClearedHex(CooldownsClearedFact const& fact)
    {
        WorldPacket packet;
        BuildClearCooldownsPacket(packet, fact);
        return Hex(packet);
    }

    /// Stands in for the session: records "<opcode>:<hex>" per packet it is sent.
    struct FakeSession
    {
        std::vector<std::string> sent;

        void SendPacket(WorldPacket const* packet)
        {
            sent.push_back(Hex(*packet));
        }
    };

    /// Stands in for the character: GetSession() answers whichever session it holds now.
    struct FakeOwner
    {
        FakeSession* session = NULL;

        FakeSession* GetSession() const
        {
            return session;
        }
    };
}

// SMSG_COOLDOWN_EVENT (0x4F26): uint32 spell id, then the owner's guid as a uint64.
TEST(CooldownPackets_CooldownEventBytes)
{
    const ObjectGuid owner(kOwnerRaw);

    // 93002 = 0x00016B4A -> 4a6b0100; the guid, low byte first -> 3c00a50000ff0001.
    CHECK_STR(EventHex(Event(93002, owner)), "4f26:4a6b01003c00a50000ff0001");
    // 93005 = 0x00016B4D.
    CHECK_STR(EventHex(Event(93005, owner)), "4f26:4d6b01003c00a50000ff0001");
    CHECK_STR(EventHex(Event(93001, owner)), "4f26:496b01003c00a50000ff0001");
}

// SMSG_CLEAR_COOLDOWNS (0x59B4), the whole-map form, for guid 0x0100FF0000A5003C and the spells
// 93001, 93003, 93999:
//   bits: g1 g3 g6 = 0 0 0; the count 3 in 24 bits; g7 g5 g2 g4 g0 = 1 1 1 0 1 -- 32 bits:
//         00000000 00000000 00000000 01111101 = 00 00 00 7d
//   bytes g7 g2 g4 g5 g1 g3 (non-zero ones, each ^ 1): 01->00, a5->a4, ff->fe = 00 a4 fe
//   the spell ids, uint32 each, map order: 496b0100 4b6b0100 2f6f0100
//   bytes g0 g6: 3c->3d = 3d
TEST(CooldownPackets_ClearCooldownsBytes)
{
    const ObjectGuid owner(kOwnerRaw);

    CHECK_STR(ClearedHex(Cleared(owner, { 93001, 93003, 93999 })), "59b4:0000007d00a4fe496b01004b6b01002f6f01003d");

    // A guid of zero bytes but g0: no mask bit but g0's, and only g0's byte.
    // bits: 000, count 1 (24 bits), 0 0 0 0 1 -> 00000000 00000000 00000000 00100001 = 00 00 00 21;
    // no g7..g3 bytes; 93001 = 496b0100; g0 2a -> 2b.
    CHECK_STR(ClearedHex(Cleared(ObjectGuid(uint64(0x2A)), { 93001 })), "59b4:00000021496b01002b");
}

// The installed callbacks turn each fact into the packet above and send it to the owner's session.
TEST(CooldownPackets_InstalledCallbacksSendTheGoldenPackets)
{
    FakeSession session;
    FakeOwner owner;
    owner.session = &session;

    CooldownEventSink sendEvent = CooldownEventToSession(&owner);
    CooldownsClearedSink sendCleared = CooldownsClearedToSession(&owner);

    sendEvent(Event(93002, ObjectGuid(kOwnerRaw)));
    sendCleared(Cleared(ObjectGuid(kOwnerRaw), { 93001, 93003, 93999 }));
    sendEvent(Event(93005, ObjectGuid(kOwnerRaw)));
    sendCleared(Cleared(ObjectGuid(uint64(0x2A)), { 93001 }));

    REQUIRE(session.sent.size() == 4u);
    CHECK_STR(session.sent[0], "4f26:4a6b01003c00a50000ff0001");
    CHECK_STR(session.sent[1], "59b4:0000007d00a4fe496b01004b6b01002f6f01003d");
    CHECK_STR(session.sent[2], "4f26:4d6b01003c00a50000ff0001");
    CHECK_STR(session.sent[3], "59b4:00000021496b01002b");
}

// The session is read at each send, never kept: a callback made before the owner's session
// changed sends to the new one.
TEST(CooldownPackets_InstalledCallbacksReadTheSessionAtEachSend)
{
    FakeSession first;
    FakeSession second;
    FakeOwner owner;
    owner.session = &first;

    CooldownEventSink sendEvent = CooldownEventToSession(&owner);
    CooldownsClearedSink sendCleared = CooldownsClearedToSession(&owner);

    sendEvent(Event(93001, ObjectGuid(kOwnerRaw)));
    owner.session = &second;
    sendEvent(Event(93002, ObjectGuid(kOwnerRaw)));
    sendCleared(Cleared(ObjectGuid(uint64(0x2A)), { 93001 }));

    REQUIRE(first.sent.size() == 1u);
    CHECK_STR(first.sent[0], "4f26:496b01003c00a50000ff0001");
    REQUIRE(second.sent.size() == 2u);
    CHECK_STR(second.sent[0], "4f26:4a6b01003c00a50000ff0001");
    CHECK_STR(second.sent[1], "59b4:00000021496b01002b");
}
