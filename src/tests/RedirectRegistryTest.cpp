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

#include "RedirectRegistry.h"

#include <chrono>
#include <thread>

namespace
{
    proto::RedirectTicket Ticket(const char* address, proto::SessionId session)
    {
        proto::RedirectTicket ticket;
        ticket.clientAddress = address;
        ticket.session       = session;
        return ticket;
    }
}

TEST(RedirectRegistry_claim_consumes_the_ticket)
{
    proto::RedirectRegistry registry(std::chrono::milliseconds(1000));

    CHECK(registry.Open(Ticket("198.51.100.4", 7)));
    CHECK_EQ(registry.InFlightCount(), uint32(1));

    proto::RedirectTicket claimed;
    CHECK(registry.Claim("198.51.100.4", claimed));
    CHECK_EQ(claimed.session, proto::SessionId(7));
    CHECK_EQ(registry.InFlightCount(), uint32(0));

    // A second connection from the same client has nothing left to take, which
    // is what stops one socket being adopted as two sessions' second stream.
    CHECK(!registry.Claim("198.51.100.4", claimed));
}

TEST(RedirectRegistry_claim_ignores_the_ephemeral_port)
{
    // The redirected connection arrives from a different source port than the
    // one that is already logged in, so an address that carried a port would
    // never match. Two sockets, same client, different ports: the first claims,
    // and the second finds nothing -- which is the serialisation working, not a
    // lookup failing.
    proto::RedirectRegistry registry(std::chrono::milliseconds(1000));

    CHECK(registry.Open(Ticket("203.0.113.9", 11)));

    proto::RedirectTicket claimed;
    CHECK(registry.Claim("203.0.113.9", claimed));
    CHECK_EQ(claimed.session, proto::SessionId(11));

    CHECK(!registry.Claim("203.0.113.9", claimed));
}

TEST(RedirectRegistry_one_redirect_in_flight_per_address)
{
    // Two players behind one household router present the same address, and the
    // arriving socket carries nothing that tells them apart. Refusing the second
    // redirect makes the second player wait; allowing it would give one of them
    // the other's stream.
    proto::RedirectRegistry registry(std::chrono::milliseconds(1000));

    CHECK(registry.Open(Ticket("198.51.100.4", 1)));
    CHECK(!registry.Open(Ticket("198.51.100.4", 2)));
    CHECK_EQ(registry.InFlightCount(), uint32(1));

    // A different address is unaffected.
    CHECK(registry.Open(Ticket("198.51.100.5", 3)));
    CHECK_EQ(registry.InFlightCount(), uint32(2));

    proto::RedirectTicket claimed;
    CHECK(registry.Claim("198.51.100.4", claimed));
    CHECK_EQ(claimed.session, proto::SessionId(1));

    // Once the first completes, the second player's redirect can go out.
    CHECK(registry.Open(Ticket("198.51.100.4", 2)));
}

TEST(RedirectRegistry_cancel_withdraws_by_session)
{
    proto::RedirectRegistry registry(std::chrono::milliseconds(1000));

    CHECK(registry.Open(Ticket("198.51.100.4", 5)));
    registry.Cancel(5);
    CHECK_EQ(registry.InFlightCount(), uint32(0));

    // Which is what lets a session reissue its own redirect without waiting for
    // the first to time out.
    CHECK(registry.Open(Ticket("198.51.100.4", 5)));
}

TEST(RedirectRegistry_expires_a_client_that_never_came_back)
{
    proto::RedirectRegistry registry(std::chrono::milliseconds(1));

    CHECK(registry.Open(Ticket("198.51.100.4", 9)));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    CHECK_EQ(registry.ExpireStale(), size_t(1));
    CHECK_EQ(registry.InFlightCount(), uint32(0));

    proto::RedirectTicket claimed;
    CHECK(!registry.Claim("198.51.100.4", claimed));
}
