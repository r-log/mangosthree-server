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

#include "SessionLinks.h"

#include <memory>
#include <string>
#include <vector>

using proto::LinkSlot;

namespace
{
    /// A link that records what it was asked to send instead of sending it.
    class RecordingLink : public proto::IClientLink
    {
        public:

            explicit RecordingLink(const std::string& address)
                : m_address(address), m_closed(false)
            {
            }

            void SendPacket(const WorldPacket& packet) override
            {
                sent.push_back(packet.GetOpcode());
            }

            void Close() override { m_closed = true; }

            const std::string& GetRemoteAddress() const override { return m_address; }

            bool IsClosed() const override { return m_closed; }

            std::vector<uint16> sent;

        private:

            std::string m_address;
            bool        m_closed;
    };

    WorldPacket Packet(uint16 opcode)
    {
        return WorldPacket(opcode, 0);
    }

    // Which stream each of these belongs on is the client's decision, read out
    // of its own tables; they are named here only so the cases read as intent.
    //
    // Stream 1 carries exactly the nineteen opcodes the client's receive gate
    // insists on -- combat, loot and duel. Everything else goes on stream 0,
    // INCLUDING packets the client would itself transmit on stream 1: its send
    // router and its receive gate are different tables, and only the gate binds
    // the server. SMSG_UPDATE_OBJECT and SMSG_LOGIN_VERIFY_WORLD were listed
    // here as stream 1 on the strength of the send router, and the live client
    // dropped the stream over it (see OpcodeSlots.h).
    const uint16 ON_STREAM_ZERO       = SMSG_TIME_SYNC_REQ;
    const uint16 ON_STREAM_ZERO_BULK  = SMSG_PLAYER_MOVE;
    const uint16 ON_STREAM_ZERO_WORLD = SMSG_UPDATE_OBJECT;
    const uint16 ON_STREAM_ZERO_LOGIN = SMSG_LOGIN_VERIFY_WORLD;
    const uint16 ON_STREAM_ONE        = SMSG_ATTACKSTART;
    const uint16 ON_STREAM_ONE_LOOT   = SMSG_LOOT_RESPONSE;
    const uint16 ON_STREAM_ONE_GATED  = SMSG_ATTACKSTART;
}

TEST(SessionLinks_routes_by_opcode)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    links.SendPacket(Packet(ON_STREAM_ZERO_BULK));
    links.SendPacket(Packet(ON_STREAM_ONE));
    links.SendPacket(Packet(ON_STREAM_ZERO));

    CHECK_EQ(zero->sent.size(), size_t(2));
    CHECK_EQ(one->sent.size(), size_t(1));
    CHECK_EQ(uint32(one->sent[0]), uint32(ON_STREAM_ONE));
}

TEST(SessionLinks_keeps_the_login_sequence_on_stream_zero)
{
    // The regression, and the first login this fork ever completed to the
    // loading screen. These four are the tail of the login sequence; every one
    // of them is send_slot 1 in the client's own router and recv_slot 0. Sent on
    // the second stream they cost the session its stream and produced
    // CMSG_LOG_DISCONNECT reason 3 -- the client will not take them there.
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    links.SendPacket(Packet(SMSG_ADDON_INFO));
    links.SendPacket(Packet(SMSG_TUTORIAL_FLAGS));
    links.SendPacket(Packet(SMSG_ACCOUNT_DATA_TIMES));
    links.SendPacket(Packet(ON_STREAM_ZERO_LOGIN));
    links.SendPacket(Packet(ON_STREAM_ZERO_WORLD));

    CHECK_EQ(zero->sent.size(), size_t(5));
    CHECK_EQ(one->sent.size(), size_t(0));

    // And the nineteen still go where the gate demands.
    links.SendPacket(Packet(ON_STREAM_ONE));
    links.SendPacket(Packet(ON_STREAM_ONE_LOOT));

    CHECK_EQ(zero->sent.size(), size_t(5));
    CHECK_EQ(one->sent.size(), size_t(2));
}

TEST(SessionLinks_holds_stream_one_traffic_until_it_is_live)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    proto::SessionLinks links(zero);

    CHECK(links.IsSlotLive(LinkSlot::Zero));
    CHECK(!links.IsSlotLive(LinkSlot::One));

    links.SendPacket(Packet(ON_STREAM_ONE));
    links.SendPacket(Packet(ON_STREAM_ONE_LOOT));
    links.SendPacket(Packet(ON_STREAM_ZERO_BULK));

    // Nothing bound for the second stream leaked onto the first: the client
    // would have discarded it, and it is not lost either.
    CHECK_EQ(zero->sent.size(), size_t(1));
    CHECK_EQ(links.GetCounters().emittedBeforeLive, uint32(2));

    auto one = std::make_shared<RecordingLink>("198.51.100.4");
    links.AttachSlotOne(one, 0);

    CHECK_EQ(one->sent.size(), size_t(2));
    CHECK_EQ(uint32(one->sent[0]), uint32(ON_STREAM_ONE));
    CHECK_EQ(uint32(one->sent[1]), uint32(ON_STREAM_ONE_LOOT));
}

TEST(SessionLinks_refuses_a_gated_opcode_on_stream_zero)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    links.SendOn(LinkSlot::Zero, Packet(ON_STREAM_ONE_GATED));

    CHECK_EQ(zero->sent.size(), size_t(0));
    CHECK_EQ(one->sent.size(), size_t(0));
    CHECK_EQ(links.GetCounters().slotGateViolations, uint32(1));

    // Routed normally it reaches the stream the client is watching.
    links.SendPacket(Packet(ON_STREAM_ONE_GATED));
    CHECK_EQ(one->sent.size(), size_t(1));
}

TEST(SessionLinks_losing_stream_one_does_not_fall_back)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);
    links.DetachSlotOne(one.get());

    CHECK(!links.IsSlotLive(LinkSlot::One));

    links.SendPacket(Packet(ON_STREAM_ONE));

    // Held for the stream that is coming back, not redirected onto the one that
    // is up. A client that received it there would drop it silently.
    CHECK_EQ(zero->sent.size(), size_t(0));
    CHECK_EQ(one->sent.size(), size_t(0));

    auto reopened = std::make_shared<RecordingLink>("198.51.100.4");
    links.AttachSlotOne(reopened, 0);
    CHECK_EQ(reopened->sent.size(), size_t(1));
}

TEST(SessionLinks_session_lifetime_follows_stream_zero)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    one->Close();
    CHECK(!links.IsClosed());

    zero->Close();
    CHECK(links.IsClosed());
}

TEST(SessionLinks_close_tears_down_both)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);
    links.Close();

    CHECK(zero->IsClosed());
    CHECK(one->IsClosed());
}

TEST(SessionLinks_refuses_a_stream_one_that_answers_a_superseded_redirect)
{
    // Two redirects went out (the first client socket stalled, the session
    // reissued). The socket answering the first must not become the live stream
    // over the one answering the second, and its late close must not take the
    // live stream down.
    std::shared_ptr<RecordingLink> zero   = std::make_shared<RecordingLink>("198.51.100.1");
    std::shared_ptr<RecordingLink> first  = std::make_shared<RecordingLink>("198.51.100.1");
    std::shared_ptr<RecordingLink> second = std::make_shared<RecordingLink>("198.51.100.1");
    proto::SessionLinks links(zero);

    links.ExpectSlotOne(1);
    links.ExpectSlotOne(2);                              // reissued
    CHECK(!links.AttachSlotOne(first, 1));               // the straggler
    CHECK(!links.IsSlotLive(proto::LinkSlot::One));
    CHECK(links.AttachSlotOne(second, 2));
    CHECK(links.IsSlotLive(proto::LinkSlot::One));

    links.DetachSlotOne(first.get());                    // the straggler closes late
    CHECK(links.IsSlotLive(proto::LinkSlot::One));
    links.DetachSlotOne(second.get());
    CHECK(!links.IsSlotLive(proto::LinkSlot::One));
}

TEST(SessionLinks_sends_a_pinned_packet_on_the_stream_it_names)
{
    // A reply the client times per-connection has to go back on the wire its
    // request arrived on. Opcode routing cannot say that: SMSG_PONG is one
    // opcode whichever stream asked for it, and ServerSlotOf puts it on 0.
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    WorldPacket pinned = Packet(ON_STREAM_ZERO);   // routes to 0 by opcode
    pinned.SetStream(LinkSlot::One);               // but was asked for on 1
    links.SendPacket(pinned);

    CHECK_EQ(one->sent.size(), size_t(1));
    CHECK_EQ(zero->sent.size(), size_t(0));
    CHECK_EQ(uint32(one->sent[0]), uint32(ON_STREAM_ZERO));
}

TEST(SessionLinks_unpinned_packets_still_route_by_opcode)
{
    // The pin is an exception, not a new policy: everything that does not ask
    // for a stream must keep going where its opcode says.
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    auto one  = std::make_shared<RecordingLink>("198.51.100.4");

    proto::SessionLinks links(zero);
    links.AttachSlotOne(one, 0);

    WorldPacket plain = Packet(ON_STREAM_ZERO);
    CHECK(!plain.HasStream());
    links.SendPacket(plain);

    CHECK_EQ(zero->sent.size(), size_t(1));
    CHECK_EQ(one->sent.size(), size_t(0));
}

TEST(WorldPacket_carries_its_stream_across_a_copy)
{
    // Every inbound packet is copied once before any handler sees it:
    // WorldGateway enqueues `new WorldPacket(std::move(packet))`, and because
    // WorldPacket declares a copy constructor that move resolves to a copy.
    // A copy constructor that forgot this field would drop the stream exactly
    // between the connection that knows it and the handler that needs it,
    // leaving the pong on stream 0 and the World latency at zero -- with
    // nothing in the routing code looking wrong.
    WorldPacket arrived(ON_STREAM_ZERO, 0);
    arrived.SetStream(LinkSlot::One);

    WorldPacket copied(arrived);

    CHECK(copied.HasStream());
    CHECK(copied.GetStream() == LinkSlot::One);
}

TEST(WorldPacket_has_no_stream_until_one_is_set)
{
    WorldPacket fresh(ON_STREAM_ZERO, 0);
    CHECK(!fresh.HasStream());

    fresh.SetStream(LinkSlot::One);
    CHECK(fresh.HasStream());

    // Reusing the buffer discards the provenance with the contents.
    fresh.Initialize(ON_STREAM_ZERO, 0);
    CHECK(!fresh.HasStream());
}
