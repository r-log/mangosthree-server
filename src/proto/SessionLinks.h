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

#include "IClientLink.h"
#include "LinkSlot.h"
#include "Platform/Define.h"
#include "WorldPacket.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>

namespace proto
{
    /**
     * @brief The two world streams of one session, behind a single link.
     *
     * A 15595 client does not hold one connection to the world; it holds two, and
     * it decides which of them a given opcode belongs on by consulting a table
     * baked into its own binary. Nineteen opcodes it will not even look at unless
     * they arrive on stream 1. So "send this packet to this player" is a routing
     * decision, not a socket write -- and it is a decision no caller in the game
     * should have to make or could reliably make.
     *
     * Hence this: it implements the same IClientLink the world already sends
     * through, and does the routing underneath. Game code keeps calling
     * SendPacket() and does not learn that a second socket exists.
     *
     * The interesting states are the ones before stream 1 is usable. Between the
     * redirect going out and the client promoting the new connection, the client
     * holds every stream-1 packet in a queue of its own and flushes it only on
     * promotion. Sending into that window is not an error the client reports; the
     * packets simply sit there, and if promotion never happens they are never
     * seen. This class mirrors that queue on the server side so the same window
     * costs a delay rather than a loss, and counts every entry into it -- in a
     * correct deployment nothing should be emitted for stream 1 before it is
     * live, so a non-zero count is a real defect somewhere upstream.
     */
    class SessionLinks : public IClientLink
    {
        public:

            explicit SessionLinks(std::shared_ptr<IClientLink> streamZero);

            // --- IClientLink ---------------------------------------------------

            /// Route by opcode and send. This is the path all game code takes.
            void SendPacket(const WorldPacket& packet) override;

            /// Tear down both streams.
            void Close() override;

            const std::string& GetRemoteAddress() const override;

            /// True once stream 0 is gone. Stream 0 is the session's lifetime;
            /// losing stream 1 is a recoverable fault, not the end of the session.
            bool IsClosed() const override;

            bool IsSlotLive(LinkSlot slot) const override;

            // --- Dual-stream control -------------------------------------------

            /**
             * @brief Send on a named stream, bypassing the opcode table.
             *
             * For the handful of packets whose stream is decided by the state of
             * the handshake rather than by which opcode they are.
             */
            void SendOn(LinkSlot slot, const WorldPacket& packet);

            /**
             * @brief Adopt a redirected connection as stream 1 and release the queue.
             *
             * Called once the client has proved it promoted the connection, which
             * it does by framing a packet on it -- not by completing the banner,
             * which happens while the stream is still held.
             */
            /// Attach the client's second stream. Refused (false) when `generation`
            /// is not the one last announced with ExpectSlotOne: the socket answers a
            /// redirect that has since been reissued.
            bool AttachSlotOne(const std::shared_ptr<IClientLink>& link, uint32 generation);
            /// The generation of the redirect currently in flight; older sockets are refused.
            void ExpectSlotOne(uint32 generation);

            /// Stream 1 died. Anything routed there queues again rather than
            /// falling back onto stream 0, which the client would discard.
            /// Detach the second stream -- only if `link` is the one attached, so a
            /// stale socket closing late cannot take the live stream down with it.
            void DetachSlotOne(const IClientLink* link);

            struct Counters
            {
                uint32 slotGateViolations = 0; ///< a stream-1-only opcode aimed elsewhere
                uint32 emittedBeforeLive  = 0; ///< queued because stream 1 was not up
                uint32 droppedWhileHeld   = 0; ///< queue full; these are lost
            };

            Counters GetCounters() const;

        private:

            /// Caller holds the lock.
            void QueueForSlotOne(const WorldPacket& packet);

            mutable std::mutex m_lock;

            std::shared_ptr<IClientLink> m_zero;
            std::shared_ptr<IClientLink> m_one;
            uint32 m_expectedGeneration = 0;

            std::deque<WorldPacket> m_held;

            Counters m_counters;
    };
}
