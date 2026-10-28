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

#ifndef MANGOS_H_WHOHANDLERS
#define MANGOS_H_WHOHANDLERS

#include <optional>
#include <string>
#include "Platform/Define.h"
#include "SessionId.h"

struct WhoisRow;
class WorldPacket;
class WorldSession;

/// The client's who opcodes: the who list of the players online who match the client's filters,
/// and an administrator's whois of the account behind a character. Static entry points the opcode
/// table binds, each borrowing the session for one call and storing nothing. The whois is answered
/// a tick later from the account's row by a continuation, which re-finds the session by its
/// identities.
struct WhoHandlers
{
    public:
        static void HandleWho(WorldSession& session, WorldPacket& recv_data);
        static void HandleWhois(WorldSession& session, WorldPacket& recv_data);

    private:
        /// Answers CMSG_WHOIS from the `account` row the handler queues. The opcode is
        /// SEC_ADMINISTRATOR-gated but is not a chat command, so no AdminScope can cover
        /// it and it is a continuation instead.
        static void HandleWhoisCallback(std::optional<WhoisRow> const& row, uint32 accountId,
                                        proto::SessionId sessionId, std::string charname);
};

#endif
