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

#ifndef MANGOS_PROTO_CLIENTCONNECTION_H
#define MANGOS_PROTO_CLIENTCONNECTION_H

#include <cstdint>
#include <utility>
#include "IClientLink.h"
#include "IWorldGateway.h"
#include "LinkSlot.h"
#include "PacketCodec.h"
#include "RedirectRegistry.h"

#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"
#include "net/ISession.hpp"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace proto
{
    class SessionLinks;

    /**
     * @brief What one acceptor makes of the connections it takes.
     *
     * The two world ports differ in what a connection arriving on them means,
     * not in how it is framed: one is a client arriving to log in, the other is a
     * client the server has already told to come back on a second socket.
     */
    struct EndpointPolicy
    {
        ConnRole role = ConnRole::Live0;

        /**
         * @brief Whether the redirected connection gets its own ciphers.
         *
         * The client re-derives a fresh cipher pair for the second stream from
         * the session key it already holds -- it does not authenticate again --
         * but what prompts it to do so is not settled. The only candidate among
         * the packets a server may write to a connection that is not yet a
         * stream is SMSG_AUTH_CHALLENGE, which the client accepts on any
         * connection and dispatches ahead of everything else.
         *
         * So this sends that challenge and arms both directions once the client
         * answers the banner. Turn it off to leave the second stream in plain
         * text, which is what a client that does not arm on the challenge will
         * expect -- the symptom of getting this wrong is a stream-1 socket that
         * goes quiet immediately after the banner.
         */
        bool armRedirectedCrypto = true;
    };

    /**
     * @brief One client connection, speaking the 4.3.4 world protocol.
     *
     * This is the whole of what used to be WorldSocket, minus everything that was
     * never protocol: no database, no configuration, no session object, no Warden,
     * no scripting hooks. What is left is exactly four jobs -- run the Cata
     * pre-auth transport handshake, frame the stream, run the header cipher, and
     * prove the client is who it claims -- plus handing the results to the world
     * through IWorldGateway.
     *
     * Threading: the transport calls OnConnect/OnData/OnClose on a network thread,
     * one connection at a time. SendPacket() may be called from any thread (the
     * world update thread does, constantly), so header encryption is serialised and
     * the byte hand-off goes through net::Sender, which the transport disarms at
     * teardown -- a world thread still ticking a dying session merely sends into a
     * no-op rather than touching a freed socket.
     */
    class ClientConnection : public net::ISession, public IClientLink
    {
        public:

            ClientConnection(IWorldGateway& gateway, RedirectRegistry& redirects,
                             const EndpointPolicy& policy);
            ~ClientConnection() override;

            // --- net::ISession ------------------------------------------------

            void setPeerAddress(const std::string& address) override
            {
                m_address = address;
            }

            void setSender(net::Sender sender) override
            {
                m_sender = std::move(sender);
            }

            void setCloser(net::Closer closer) override
            {
                m_closer = std::move(closer);
            }

            std::vector<uint8_t> onConnect() override;
            std::vector<uint8_t> onData(const uint8_t* data, size_t len) override;
            void onClose() override;

            bool closed() const override
            {
                return m_closed.load(std::memory_order_acquire);
            }

            // --- IClientLink (what the world may do to us) --------------------

            /// Encode, encrypt and queue a packet. Safe from any thread, and a
            /// no-op once the peer is gone.
            void SendPacket(const WorldPacket& packet) override;

            /// Mark the connection dead and ask the transport to tear it down.
            void Close() override;

            const std::string& GetRemoteAddress() const override { return m_address; }

            bool IsClosed() const override { return closed(); }

            /// Sockets currently open, for the mangosd console/window title.
            static uint32 GetOpenConnectionCount()
            {
                return s_openConnections.load(std::memory_order_relaxed);
            }

        private:

            /// Handle the client's own MSG_WOW_CONNECTION. Read and dropped --
            /// WorldSocket.cpp's HandleWowConnection never validated its content
            /// either, and onConnect() has already sent the challenge by the time
            /// this can possibly arrive (see onConnect()'s doc comment).
            void HandleWowConnection(WorldPacket& packet);

            /// Dispatch one fully decoded packet. Returns false to drop the peer.
            bool HandlePacket(WorldPacket&& packet);

            bool HandleAuthSession(WorldPacket& packet);

            /// The 47-byte greeting every world connection opens with, on either
            /// port. It is a packet whose opcode happens to spell two of its own
            /// letters, so it goes through the codec like anything else.
            void AppendBanner(std::vector<uint8_t>& wire);

            void AppendAuthChallenge(std::vector<uint8_t>& wire);

            /// Find the session this redirected connection belongs to. Returns
            /// false if nothing is expecting a second stream from this client.
            bool ClaimRedirect();

            /**
             * @brief Derive this connection's ciphers from the session key.
             *
             * The redirected socket never sees a login, so it has nothing of its
             * own to key from; it re-uses the key the session already agreed on
             * and starts fresh cipher state, exactly as the client does. Called
             * once the banner reply is in, because the banner itself is plain
             * text in both directions.
             */
            void ArmRedirectedCrypto();

            /**
             * @brief Take this connection to be the session's stream 1.
             *
             * The signal is the client framing anything at all on this socket
             * after the banner. Nothing weaker will do: the banner exchange
             * happens while the connection is still staged, so a completed banner
             * says the TCP works, not that the client has adopted it. Only after
             * promotion does the client route packets here, so a packet arriving
             * here is the promotion, observed rather than assumed.
             *
             * @return false if the session is gone, in which case drop the peer.
             */
            bool PromoteToSlotOne();

            /// Send a bare, bit-packed SMSG_AUTH_RESPONSE carrying only a status
            /// byte. Cata's failure response is `WriteBit(false); WriteBit(false);
            /// << uint8(status)` (WorldSocket.cpp:1038-1041 and three further call
            /// sites) -- NOT a plain byte stream, so MangosTwo's SendAuthStatus
            /// cannot be copied even though the AuthStatus values coincide.
            void SendAuthStatus(AuthStatus status);

            IWorldGateway&    m_gateway;
            RedirectRegistry& m_redirects;

            EndpointPolicy m_policy;

            /// Starts at the policy's role and advances to Live1 on promotion.
            ConnRole m_role;

            /**
             * @brief Set on a redirected connection: the session's pair of streams.
             *
             * Weak, and both halves of that matter. Once promoted, the streams
             * hold this connection, so holding them back would be a cycle that
             * outlives the session. And a session that goes away while its
             * redirect is in flight should leave this connection with nothing to
             * join -- which is exactly what a expired weak reference says.
             */
            std::weak_ptr<SessionLinks> m_links;

            /// The session key of the session being rejoined. Meaningful on a
            /// redirected connection only.
            BigNumber m_redirectKey;
            /// The generation of the redirect this staging socket answers.
            uint32 m_redirectGeneration = 0;

            /// Whether the banner exchange on this connection has completed.
            bool m_bannerDone;

            std::string m_address;

            PacketCodec m_codec;

            AuthCrypt  m_crypt;
            std::mutex m_cryptSendLock; ///< serialises header encryption on send

            /// Server half of the authentication nonce, drawn from the OpenSSL RNG
            /// rather than the general-purpose PRNG (the old WorldSocket used
            /// rand32()): it is an input to the client's SHA-1 proof, so a
            /// predictable value weakens the handshake.
            uint32 m_seed;

            SessionId m_session;

            /// The same id, readable off the network thread. SendPacket runs on the
            /// world thread while m_session is written here on the network thread, so
            /// the trace needs a copy it can read without a race. Tracing only --
            /// m_session stays the authority.
            std::atomic<SessionId> m_traceSession;

            std::atomic<bool> m_closed;

            net::Sender m_sender;
            net::Closer m_closer;

            static std::atomic<uint32> s_openConnections;
    };
}

#endif
