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

// A blocking client socket with a read deadline. Deliberately not the tree's own
// `net` engine: this is the side of the wire the server does NOT implement, and
// borrowing the server's transport to test the server would hide exactly the
// class of fault -- a framing or ordering disagreement between the two ends --
// that a synthetic client exists to find.

#include "Platform/Define.h"

#include <cstddef>
#include <string>
#include <vector>

namespace loadtest
{
    /// One-time process setup (WSAStartup on Windows, nothing elsewhere).
    bool InitSockets(std::string& error);
    void ShutdownSockets();

    class Socket
    {
        public:

            Socket();
            ~Socket();

            Socket(const Socket&) = delete;
            Socket& operator=(const Socket&) = delete;

            /// @param host  dotted-quad or resolvable name
            /// @param timeoutMs  deadline for the connect itself
            bool Connect(const std::string& host, uint16 port, int timeoutMs,
                         std::string& error);

            /// Writes every byte or fails; partial writes are retried.
            bool SendAll(const uint8* data, size_t len, std::string& error);

            /**
             * @brief Wait up to `timeoutMs` for readable bytes and take what is there.
             *
             * @param out    receives whatever arrived; untouched on timeout
             * @return false on error or on a peer that closed the connection.
             *         A timeout is NOT a failure: `out` is simply left empty, so
             *         callers that are pumping rather than waiting for one packet
             *         can poll with a short deadline and keep going.
             */
            bool Recv(std::vector<uint8>& out, int timeoutMs, std::string& error);

            /// True once the peer closed its half. Set by Recv.
            bool PeerClosed() const { return m_peerClosed; }

            bool IsOpen() const;
            void Close();

        private:

#ifdef _WIN32
            uintptr_t m_fd;
#else
            int m_fd;
#endif
            bool m_peerClosed;
    };
}
