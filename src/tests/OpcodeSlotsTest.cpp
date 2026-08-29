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

#include "TestHarness.h"

#include "OpcodeSlots.h"

#include <vector>

using proto::LinkSlot;

namespace
{
    /// The opcodes the client's receive gate drops unless they arrive on the
    /// second stream. Written out rather than derived from the generated table,
    /// because the point of the test is that the table still says this: a
    /// regeneration that quietly loses one of them costs a fifth of combat.
    const uint16 RECV_GATED[] =
    {
        0x2D15, // SMSG_ATTACK_START
        0x0934, // SMSG_ATTACK_STOP
        0x4504, // SMSG_DUEL_REQUESTED
        0x2136, // CMSG_DUEL_ACCEPTED
        0x6624, // CMSG_DUEL_CANCELLED
        0x2527, // SMSG_DUEL_COMPLETE
        0x2D36, // SMSG_DUEL_WINNER
        0x0A27, // SMSG_DUEL_IN_BOUNDS
        0x0C26, // SMSG_DUEL_OUT_OF_BOUNDS
        0x4C16, // SMSG_LOOT_RESPONSE
        0x2836, // SMSG_LOOT_MONEY_NOTIFY
        0x2B37, // SMSG_LOOT_CLEAR_MONEY
        0x6D15, // SMSG_LOOT_ITEM_NOTIFY
        0x6817, // SMSG_LOOT_REMOVED
        0x6D25, // SMSG_LOOT_RELEASE
        0x0E15, // SMSG_ITEM_PUSH_RESULT
        0x2115, // SMSG_QUEST_GIVER_STATUS
        0x2124, // SMSG_QUERY_TIME_RESPONSE
        0x2225  // SMSG_MOUNT_RESULT
    };

    /// The connection-control class: accepted on any connection, live or
    /// staging, and dispatched ahead of the receive gate.
    const uint16 CONTROL[] =
    {
        0x0140, // SMSG_RESUME_COMMS
        0x0142, // SMSG_RESET_COMPRESSION_CONTEXT
        0x0542, // SMSG_FLOOD_DETECTED
        0x0942, // SMSG_CONNECT_TO
        0x4140, // SMSG_SUSPEND_COMMS
        0x4542, // SMSG_AUTH_CHALLENGE
        0x4D40, // SMSG_DROP_NEW_CONNECTION
        0x4D42  // SMSG_PONG
    };
}

TEST(OpcodeSlots_recv_gated_set_is_exactly_the_nineteen)
{
    for (uint16 opcode : RECV_GATED)
    {
        CHECK(proto::IsRecvGated(opcode));
    }

    uint32 gated = 0;
    for (uint32 opcode = 0; opcode <= 0xFFFF; ++opcode)
    {
        if (proto::IsRecvGated(uint16(opcode)))
        {
            ++gated;
        }
    }

    CHECK_EQ(gated, uint32(sizeof(RECV_GATED) / sizeof(RECV_GATED[0])));
}

TEST(OpcodeSlots_recv_gated_all_send_on_stream_one)
{
    // Routing sends by send_slot is an inference; this is where the client can
    // prove it wrong, and does not. If a regenerated table ever breaks it, the
    // inference dies with it and only the gate survives.
    for (uint16 opcode : RECV_GATED)
    {
        CHECK(proto::SendSlotOf(opcode) == LinkSlot::One);
    }
}

TEST(OpcodeSlots_control_class_is_exactly_the_eight)
{
    for (uint16 opcode : CONTROL)
    {
        CHECK(proto::IsTransportPlane(opcode));
        CHECK(proto::IsKnownSlotOpcode(opcode));

        // They travel on the first stream and are never gated, which is what
        // lets them address a connection that is not yet a stream.
        CHECK(proto::SendSlotOf(opcode) == LinkSlot::Zero);
        CHECK(!proto::IsRecvGated(opcode));
    }

    uint32 control = 0;
    for (uint32 opcode = 0; opcode <= 0xFFFF; ++opcode)
    {
        if (proto::IsTransportPlane(uint16(opcode)) &&
            proto::IsKnownSlotOpcode(uint16(opcode)))
        {
            ++control;
        }
    }

    CHECK_EQ(control, uint32(sizeof(CONTROL) / sizeof(CONTROL[0])));
}

TEST(OpcodeSlots_known_routing)
{
    // Login travels on the first stream; what it answers with does not, which is
    // the whole reason the second one has to be open before it runs.
    CHECK(proto::SendSlotOf(0x05B1) == LinkSlot::Zero); // CMSG_PLAYER_LOGIN
    CHECK(proto::SendSlotOf(0x2005) == LinkSlot::One);  // SMSG_LOGIN_VERIFY_WORLD
    CHECK(proto::SendSlotOf(0x4715) == LinkSlot::One);  // SMSG_UPDATE_OBJECT

    // The position feed is the busiest thing on the wire and it is NOT on the
    // second stream, whatever "stream 1 is the world stream" suggests.
    CHECK(proto::SendSlotOf(0x79A2) == LinkSlot::Zero); // SMSG_MOVE_UPDATE

    CHECK(proto::SendSlotOf(0x444D) == LinkSlot::Zero); // CMSG_PING
    CHECK(proto::SendSlotOf(0x3CA4) == LinkSlot::Zero); // SMSG_TIME_SYNC_REQ
    CHECK(proto::SendSlotOf(0x3B0C) == LinkSlot::One);  // CMSG_TIME_SYNC_RESP
}

TEST(OpcodeSlots_unknown_opcode_never_routes_to_stream_one)
{
    // Guessing an unlisted opcode onto the second stream would put it where the
    // client is not looking. Falling back to the first is the safe direction.
    for (uint32 opcode = 0; opcode <= 0xFFFF; ++opcode)
    {
        if (!proto::IsKnownSlotOpcode(uint16(opcode)))
        {
            CHECK(proto::SendSlotOf(uint16(opcode)) == LinkSlot::Zero);
            CHECK(!proto::IsRecvGated(uint16(opcode)));
        }
    }
}

TEST(OpcodeSlots_batch_container_is_not_a_routable_opcode)
{
    // 0x0D40 is the client's generic batch container. It is absent from the
    // catalogue, so it has no send_slot of its own; anything that batches has to
    // choose the stream from what it is batching, never from the container.
    CHECK(!proto::IsKnownSlotOpcode(0x0D40));
}
