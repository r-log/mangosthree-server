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

// The protocol peer's pure parts: the units the loadtest client feeds with a
// clock value and sends the output of. Nothing here opens a socket.

#include "TestHarness.h"

#include "Peer.hpp"
#include "TimeSync.hpp"
#include "Opcodes.h"
#include "WorldPacket.h"

TEST(TimeSync_answers_the_request_with_the_counter_and_the_client_clock)
{
    WorldPacket request(SMSG_TIME_SYNC_REQ, 4);
    request << uint32(7);

    uint32 counter = 0;
    REQUIRE(loadtest::ReadTimeSyncRequest(request, counter));
    CHECK_EQ(counter, uint32(7));

    const WorldPacket reply = loadtest::MakeTimeSyncResponse(counter, 123456);
    CHECK_EQ(int(reply.GetOpcode()), int(CMSG_TIME_SYNC_RESP));
    CHECK_BYTES(reply.contents(), reply.size(), { 0x07, 0x00, 0x00, 0x00, 0x40, 0xE2, 0x01, 0x00 });
}

TEST(TimeSync_refuses_a_short_request)
{
    WorldPacket request(SMSG_TIME_SYNC_REQ, 2);
    request << uint16(7);
    uint32 counter = 99;
    CHECK(!loadtest::ReadTimeSyncRequest(request, counter));
    CHECK_EQ(counter, uint32(99));
}

TEST(ClientClock_starts_at_its_base_and_never_runs_backwards)
{
    loadtest::ClientClock clock(100000);
    const uint32 a = clock.Ticks();
    const uint32 b = clock.Ticks();
    CHECK(a >= 100000);
    CHECK(b >= a);
    CHECK(b - a < 1000);
}

TEST(PeerReport_defaults_are_empty)
{
    loadtest::PeerReport r;
    CHECK_EQ(r.timeSyncsAnswered, uint32(0));
    CHECK_EQ(r.walkStarts, uint32(0));
    CHECK_EQ(r.observedTarget, uint32(0));
    CHECK(r.unregisteredChanges.empty());
    CHECK_EQ(r.acksSent, uint32(0));
}
