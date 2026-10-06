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

/// The opcode table dispatches through one free-function handler type. Every row holds the
/// address of OpcodeThunk bound to its handler; a call through a row reaches that handler on the
/// session it is given, with the packet it is given. A socketless session, built as the GM
/// harness builds one, answers a CMSG_PING handed to the CMSG_PING row with one SMSG_PONG through
/// its sink, carrying the ping's sequence; the row consumes the whole packet. The slots no row
/// names hold the thunk of Handle_NULL, every slot bound to one handler holds one address, and
/// the row of an overloaded handler name holds the overload that takes the packet.

#include "TestHarness.h"
#include "OpcodeTable.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "Auth/BigNumber.h"

#include <cstring>
#include <type_traits>
#include <vector>

static_assert(std::is_same<decltype(OpcodeHandler::handler), void (*)(WorldSession&, WorldPacket&)>::value,
              "the opcode table stores a free function taking the session and the packet");

namespace
{
    void CapturePacket(void* context, WorldPacket const& packet)
    {
        static_cast<std::vector<WorldPacket>*>(context)->push_back(packet);
    }

    int CountSlotsBoundTo(void (*handler)(WorldSession&, WorldPacket&))
    {
        int slots = 0;
        for (uint32 i = 0; i < NUM_MSG_TYPES; ++i)
        {
            if (opcodeTable[i].handler == handler)
            {
                ++slots;
            }
        }
        return slots;
    }
}

TEST(OpcodeDispatch_PingRowAnswersWithThePongOfItsSequence)
{
    InitializeOpcodes();

    std::vector<WorldPacket> sent;
    WorldSession session(1, "dispatch", nullptr, nullptr, SEC_PLAYER, EXPANSION_CATA, 0, LOCALE_enUS, BigNumber());
    session.SetSocketlessSink(&CapturePacket, &sent);

    WorldPacket ping(CMSG_PING, 8);
    ping << uint32(0);
    ping << uint32(7);

    opcodeTable[CMSG_PING].handler(session, ping);

    CHECK_EQ(ping.rpos(), size_t(8));
    REQUIRE(sent.size() == 1);
    CHECK_EQ(sent[0].GetOpcode(), uint16(SMSG_PONG));
    REQUIRE(sent[0].size() == 4);
    uint32 sequence = 0;
    sent[0] >> sequence;
    CHECK_EQ(sequence, uint32(7));

    session.SetSocketlessSink(nullptr, nullptr);
}

TEST(OpcodeDispatch_PingRowHoldsThePingHandlersThunk)
{
    InitializeOpcodes();

    CHECK(opcodeTable[CMSG_PING].handler == &OpcodeThunk<&WorldSession::HandlePingOpcode>);
    CHECK(opcodeTable[CMSG_PING].handler != &OpcodeThunk<&WorldSession::Handle_NULL>);
}

TEST(OpcodeDispatch_SlotsNoRowNamesHoldTheNullHandler)
{
    InitializeOpcodes();

    int unknown = 0;
    int unboundUnknown = 0;
    for (uint32 i = 0; i < NUM_MSG_TYPES; ++i)
    {
        if (std::strcmp(opcodeTable[i].name, "UNKNOWN") == 0)
        {
            ++unknown;
            if (opcodeTable[i].handler != &OpcodeThunk<&WorldSession::Handle_NULL>)
            {
                ++unboundUnknown;
            }
        }
    }
    CHECK(unknown > 0);
    CHECK_EQ(unboundUnknown, 0);
}

TEST(OpcodeDispatch_RowsOfOneHandlerShareOneAddress)
{
    InitializeOpcodes();

    CHECK_EQ(CountSlotsBoundTo(&OpcodeThunk<&WorldSession::HandleMovementOpcodes>), 27);
}

TEST(OpcodeDispatch_OverloadedHandlerNameBindsThePacketOverload)
{
    InitializeOpcodes();

    CHECK(opcodeTable[MSG_MOVE_WORLDPORT_ACK].handler
          == &OpcodeThunk<static_cast<void (WorldSession::*)(WorldPacket&)>(&WorldSession::HandleMoveWorldportAckOpcode)>);
}
