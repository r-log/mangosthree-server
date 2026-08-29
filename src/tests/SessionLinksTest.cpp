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
    const uint16 ON_STREAM_ZERO       = SMSG_TIME_SYNC_REQ;
    const uint16 ON_STREAM_ZERO_BULK  = SMSG_PLAYER_MOVE;
    const uint16 ON_STREAM_ONE        = SMSG_UPDATE_OBJECT;
    const uint16 ON_STREAM_ONE_LOGIN  = SMSG_LOGIN_VERIFY_WORLD;
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

TEST(SessionLinks_holds_stream_one_traffic_until_it_is_live)
{
    auto zero = std::make_shared<RecordingLink>("198.51.100.4");
    proto::SessionLinks links(zero);

    CHECK(links.IsSlotLive(LinkSlot::Zero));
    CHECK(!links.IsSlotLive(LinkSlot::One));

    links.SendPacket(Packet(ON_STREAM_ONE));
    links.SendPacket(Packet(ON_STREAM_ONE_LOGIN));
    links.SendPacket(Packet(ON_STREAM_ZERO_BULK));

    // Nothing bound for the second stream leaked onto the first: the client
    // would have discarded it, and it is not lost either.
    CHECK_EQ(zero->sent.size(), size_t(1));
    CHECK_EQ(links.GetCounters().emittedBeforeLive, uint32(2));

    auto one = std::make_shared<RecordingLink>("198.51.100.4");
    links.AttachSlotOne(one, 0);

    CHECK_EQ(one->sent.size(), size_t(2));
    CHECK_EQ(uint32(one->sent[0]), uint32(ON_STREAM_ONE));
    CHECK_EQ(uint32(one->sent[1]), uint32(ON_STREAM_ONE_LOGIN));
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
