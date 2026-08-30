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

#pragma once

#include "Auth/BigNumber.h"
#include "IWorldGateway.h"
#include "Platform/Define.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace proto
{
    class SessionLinks;

    /**
     * @brief Everything a stream-1 connection needs before it can be adopted.
     *
     * The session key is here because the redirected socket re-derives its own
     * ciphers from it rather than authenticating again -- there is no second
     * login on the second stream.
     */
    struct RedirectTicket
    {
        /// The client's IP. The port is deliberately absent: the redirected
        /// connection arrives from a different ephemeral port than stream 0, so
        /// matching on a full address never succeeds.
        std::string clientAddress;

        SessionId session = INVALID_SESSION_ID;
        /// Which redirect of this session the ticket belongs to. A staging socket
        /// that answers an older one than the session's latest is a straggler and
        /// must not become the live stream.
        uint32 generation = 0;

        BigNumber sessionKey;

        /// The account this session logged in as, exactly as the client sent it.
        /// It is here because the client hashes it into the digest that opens the
        /// second stream -- SHA-1(account, session key, server seed) -- and that
        /// digest is the only thing on this socket that proves who is on it. The
        /// address it arrives from proves nothing: a ticket is claimed by address
        /// alone, so two clients behind one address can claim each other's.
        std::string accountName;

        std::shared_ptr<SessionLinks> links;
    };

    /**
     * @brief Matches a connection accepted on the stream-1 port to its session.
     *
     * The signed redirect carries no session token, and the client sends nothing
     * on the new socket that identifies it -- its first bytes are the same
     * anonymous banner every connection opens with. The only thing the two
     * sockets share is the client's address, and an address alone is not unique:
     * two players behind one household router, or one carrier-grade NAT, present
     * the same one.
     *
     * So the match is made unambiguous by serialising instead of by adding
     * identity. At most one redirect is in flight per client address at a time;
     * a second player behind the same address waits for the first to complete or
     * to time out. That costs a few seconds of login latency in a case that is
     * rare, and it removes the failure where two sessions race for one socket and
     * one of them silently gets the other's stream.
     *
     * Thread safety: connections are accepted on network threads while the world
     * thread opens tickets, so every entry point takes the lock.
     */
    class RedirectRegistry
    {
        public:

            explicit RedirectRegistry(std::chrono::milliseconds timeout);

            /**
             * @brief Announce a redirect that is about to be signed and sent.
             *
             * @return false if one is already in flight for that client address,
             *         in which case the caller must not emit the redirect: the
             *         client would refuse it, and we would have no way to tell
             *         the two apart on arrival.
             */
            bool Open(const RedirectTicket& ticket);

            /**
             * @brief Claim the ticket a newly accepted stream-1 socket belongs to.
             *
             * Removes it: a ticket is consumed by exactly one connection.
             */
            bool Claim(const std::string& clientAddress, RedirectTicket& out);

            /// Withdraw a session's ticket, on retry or on logout.
            void Cancel(SessionId session);

            /// Drop tickets whose client never came back. Returns how many went.
            size_t ExpireStale();

            uint32 InFlightCount() const;

        private:

            typedef std::chrono::steady_clock Clock;

            struct Entry
            {
                RedirectTicket   ticket;
                Clock::time_point deadline;
            };

            /// Caller holds the lock.
            void ExpireLocked(Clock::time_point now);

            mutable std::mutex m_lock;

            std::chrono::milliseconds m_timeout;

            /// Linear, because it holds one entry per login in flight -- a handful
            /// at a time even at a thousand concurrent sessions. A hash map keyed
            /// on the address would buy nothing and would still need the scan for
            /// Cancel().
            std::vector<Entry> m_entries;
    };
}
