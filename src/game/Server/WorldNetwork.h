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

#include "ConnectTo.h"
#include "Listener.h"
#include "Policies/Singleton.h"
#include "RedirectRegistry.h"
#include "WorldGateway.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Owns the world server's two listening sockets and its side of the seam.
 *
 * Two ports, because a 4.3.4 client in world holds two TCP connections and
 * decides for itself which one each opcode belongs on. The first is the one
 * clients dial from the realm list; the second is one they only ever reach after
 * the server hands them a signed instruction to open it, so it is deliberately
 * not advertised anywhere.
 *
 * The objects are built in dependency order: the gateway (the world's
 * implementation of the protocol contract), the registry that pairs a connection
 * arriving on the second port with the session that asked for it, then the two
 * listeners. Nothing else about networking survives at this level -- accepting,
 * worker threads, buffering, backpressure and teardown all belong to the shared
 * engine underneath (src/shared/net/).
 */
class WorldNetwork : public MaNGOS::Singleton<WorldNetwork>
{
        friend class MaNGOS::Singleton<WorldNetwork>;

    public:

        /**
         * @brief Bind both ports and start accepting.
         *
         * @param port       TCP port for the first stream -- the advertised one.
         * @param streamPort TCP port for the second stream. Not advertised; a
         *                   client reaches it only via a signed redirect.
         * @param bindIp     Interface to bind, or empty for all interfaces.
         * @return false if either port could not be bound.
         */
        bool Start(uint16 port, uint16 streamPort, const std::string& bindIp);

        /// Stop accepting and tear down every live connection.
        void Stop();

        /// Sockets currently open, for the mangosd console/window title.
        uint32 GetOpenConnectionCount() const;

        /**
         * @brief Ask a logged-in session's client to open its second stream.
         *
         * Registers the pending redirect and sends the signed instruction on the
         * stream the client already has. Fails, without sending, when the client
         * would refuse the packet anyway: no key configured, or another redirect
         * already in flight to the same address.
         *
         * @return false if no redirect was sent, in which case the caller decides
         *         whether to retry later or give up on the session.
         */
        bool RequestSecondStream(const proto::RedirectTicket& ticket);

        /// Withdraw a pending redirect, on retry or on the session going away.
        void CancelSecondStream(proto::SessionId session);

        /// Drop pending redirects whose client never came back.
        void ExpirePendingRedirects();

        /// True once a usable keypair is configured. Without one the second
        /// stream cannot be opened at all and no client can enter the world.
        bool CanRedirect() const { return m_signer.IsLoaded(); }

    private:

        WorldNetwork();
        ~WorldNetwork();

        /// Read the keypair, the auth blob and the advertised address out of the
        /// configuration. Logs what an operator has to patch into the client.
        bool LoadRedirectConfiguration(const std::string& bindIp);

        proto::RedirectSigner m_signer;

        /// The destination bytes written into every redirect: where the CLIENT
        /// must connect, which is this server's own advertised address and never
        /// the address the client came from.
        std::vector<uint8> m_advertisedAddress;

        uint16 m_streamPort;

        // Declaration order matters: the listeners hold references to the gateway
        // and the registry, so both must be constructed first and destroyed last.
        WorldGateway            m_gateway;
        proto::RedirectRegistry m_redirects;
        proto::Listener         m_listener;
        proto::Listener         m_streamListener;
};

#define sWorldNetwork MaNGOS::Singleton<WorldNetwork>::Instance()
