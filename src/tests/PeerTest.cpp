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

#include "Walker.hpp"
#include "wire/MovementCodec.h"
#include "wire/MovementSequences.h"

namespace
{
    Wire::MovementStatus DecodeWalk(WorldPacket& packet)
    {
        Wire::MovementStatus s;
        const Wire::DecodeResult r = Wire::Decode(packet, Wire::SequenceFor(packet.GetOpcode()), s);
        CHECK(r.ok());
        return s;
    }
}

TEST(Walker_reports_a_walk_the_way_the_client_does)
{
    // Two seconds due east at 7 yd/s with a half-second heartbeat: one start,
    // three heartbeats, one stop; the reported position advances 3.5 yd per
    // heartbeat and the packets decode with the same layouts the server reads.
    loadtest::WalkScript script;
    script.seconds = 2;
    script.leadMs = 0;
    script.headingSet = true;
    script.heading = 0.0f;
    Wire::Vec4 start;
    start.x = 100.0f;
    start.y = 200.0f;
    start.z = 30.0f;
    start.o = 1.0f;
    loadtest::Walker walker(script, 0x0000000000000123ull, start);

    CHECK(!walker.Started());
    std::vector<WorldPacket> out = walker.Advance(1000);
    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].GetOpcode()), int(CMSG_MOVE_START_FORWARD));
    Wire::MovementStatus s = DecodeWalk(out[0]);
    CHECK_EQ(s.guid, uint64(0x123));
    CHECK_EQ(s.flags, uint32(0x00000001));   // MOVEFLAG_FORWARD
    CHECK_EQ(s.time, uint32(1000));
    CHECK_EQ(s.pos.x, 100.0f);
    CHECK_EQ(s.pos.o, 0.0f);                 // the scripted heading, not the facing
    CHECK(walker.Started());

    CHECK(walker.Advance(1200).empty());     // nothing due yet
    out = walker.Advance(1500);
    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].GetOpcode()), int(MSG_MOVE_HEARTBEAT));
    s = DecodeWalk(out[0]);
    CHECK_EQ(s.pos.x, 103.5f);
    CHECK_EQ(s.pos.y, 200.0f);
    CHECK_EQ(s.flags, uint32(0x00000001));

    out = walker.Advance(2000);
    REQUIRE(out.size() == 1);
    CHECK_EQ(DecodeWalk(out[0]).pos.x, 107.0f);
    out = walker.Advance(2500);
    REQUIRE(out.size() == 1);
    CHECK_EQ(DecodeWalk(out[0]).pos.x, 110.5f);

    out = walker.Advance(3000);              // two seconds are up
    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].GetOpcode()), int(CMSG_MOVE_STOP));
    s = DecodeWalk(out[0]);
    CHECK_EQ(s.flags, uint32(0));
    CHECK_EQ(s.pos.x, 114.0f);
    CHECK(walker.Done());
    CHECK_EQ(walker.Position().x, 114.0f);

    CHECK(walker.Advance(3500).empty());     // done means silent
    CHECK_EQ(walker.Starts(), uint32(1));
    CHECK_EQ(walker.Heartbeats(), uint32(3));
    CHECK_EQ(walker.Stops(), uint32(1));
}

TEST(Walker_waits_out_its_lead_and_uses_the_facing_by_default)
{
    loadtest::WalkScript script;
    script.seconds = 1;
    script.leadMs = 3000;
    Wire::Vec4 start;
    start.o = 1.5f;
    loadtest::Walker walker(script, 0x42, start);

    CHECK(walker.Advance(0).empty());
    CHECK(walker.Advance(2999).empty());
    CHECK(!walker.Started());
    std::vector<WorldPacket> out = walker.Advance(3000);
    REQUIRE(out.size() == 1);
    CHECK_EQ(DecodeWalk(out[0]).pos.o, 1.5f);
}

TEST(Walker_with_no_seconds_never_moves)
{
    loadtest::WalkScript script;   // seconds = 0
    loadtest::Walker walker(script, 0x42, Wire::Vec4());
    CHECK(walker.Advance(10000).empty());
    CHECK(!walker.Started());
    CHECK(!walker.Done());
}

#include "AckEngine.hpp"

namespace
{
    // A change and its ack sharing one tiny layout that carries the counter --
    // the real ack layouts arrive in P1 and plug into the same engine through
    // the registry; here the lookup is injected so the engine can be pinned now.
    const uint16 kFakeChange = 0x1001;
    const uint16 kFakeAck    = 0x1002;
    const Wire::Element kCounterLayout[] =
    {
        Wire::Element::MovementCounter, Wire::Element::PositionX, Wire::Element::End
    };

    Wire::Sequence FakeLookup(uint16 opcode)
    {
        return (opcode == kFakeChange || opcode == kFakeAck) ? kCounterLayout : nullptr;
    }

    WorldPacket FakeChange(uint32 counter)
    {
        Wire::MovementStatus s;
        s.counter = counter;
        s.pos.x = 1.0f;
        WorldPacket p(kFakeChange, 8);
        Wire::Encode(p, kCounterLayout, s);
        return p;
    }

    std::vector<loadtest::ChangePair> FakePairs()
    {
        std::vector<loadtest::ChangePair> pairs;
        loadtest::ChangePair pair;
        pair.change = kFakeChange;
        pair.ack = kFakeAck;
        pairs.push_back(pair);
        return pairs;
    }

    uint32 CounterOf(WorldPacket& ack)
    {
        Wire::MovementStatus s;
        CHECK(Wire::Decode(ack, kCounterLayout, s).ok());
        return s.counter;
    }
}

TEST(AckEngine_answers_at_once_with_the_counter_echoed)
{
    loadtest::AckPolicy policy;
    loadtest::AckEngine engine(policy, &FakeLookup, FakePairs());
    CHECK(engine.IsChange(kFakeChange));
    CHECK(!engine.IsChange(kFakeAck));

    REQUIRE(engine.Plan(FakeChange(7), 1000));
    std::vector<WorldPacket> due = engine.Due(1000);
    REQUIRE(due.size() == 1);
    CHECK_EQ(int(due[0].GetOpcode()), int(kFakeAck));
    CHECK_EQ(CounterOf(due[0]), uint32(7));
    CHECK_EQ(engine.Sent(), uint32(1));
    CHECK(engine.Due(1000).empty());
}

TEST(AckEngine_delays_when_told_to)
{
    loadtest::AckPolicy policy;
    policy.mode = loadtest::AckMode::Delay;
    policy.delayMs = 250;
    loadtest::AckEngine engine(policy, &FakeLookup, FakePairs());

    REQUIRE(engine.Plan(FakeChange(3), 1000));
    CHECK(engine.Due(1000).empty());
    CHECK(engine.Due(1249).empty());
    std::vector<WorldPacket> due = engine.Due(1250);
    REQUIRE(due.size() == 1);
    CHECK_EQ(CounterOf(due[0]), uint32(3));
}

TEST(AckEngine_mismatches_the_counter_when_told_to)
{
    loadtest::AckPolicy policy;
    policy.mode = loadtest::AckMode::Mismatch;
    loadtest::AckEngine engine(policy, &FakeLookup, FakePairs());

    REQUIRE(engine.Plan(FakeChange(9), 1000));
    std::vector<WorldPacket> due = engine.Due(1000);
    REQUIRE(due.size() == 1);
    CHECK_EQ(CounterOf(due[0]), uint32(10));
}

TEST(AckEngine_drops_when_told_to_and_counts_it)
{
    loadtest::AckPolicy policy;
    policy.mode = loadtest::AckMode::Drop;
    loadtest::AckEngine engine(policy, &FakeLookup, FakePairs());

    REQUIRE(engine.Plan(FakeChange(1), 1000));
    CHECK(engine.Due(5000).empty());
    CHECK_EQ(engine.Dropped(), uint32(1));
    CHECK_EQ(engine.Sent(), uint32(0));
}

TEST(AckEngine_counts_a_change_whose_layout_is_not_registered)
{
    loadtest::AckPolicy policy;
    loadtest::AckEngine engine(policy, [](uint16) -> Wire::Sequence { return nullptr; }, FakePairs());

    CHECK(!engine.Plan(FakeChange(1), 1000));
    CHECK(engine.Due(1000).empty());
    REQUIRE(engine.Unregistered().count(kFakeChange) == 1);
    CHECK_EQ(engine.Unregistered().at(kFakeChange), uint32(1));
}

TEST(AckEngine_knows_the_real_pairs)
{
    // The table itself is data; this pins that it names real opcodes on both
    // sides and that speed changes map to the force-change acks.
    const std::vector<loadtest::ChangePair>& pairs = loadtest::KnownChangePairs();
    CHECK(pairs.size() >= 20);
    bool foundRun = false;
    for (const loadtest::ChangePair& p : pairs)
    {
        CHECK(p.change != 0);
        CHECK(p.ack != 0);
        if (p.change == SMSG_MOVE_SET_RUN_SPEED)
        {
            foundRun = (p.ack == CMSG_FORCE_RUN_SPEED_CHANGE_ACK);
        }
    }
    CHECK(foundRun);
}
