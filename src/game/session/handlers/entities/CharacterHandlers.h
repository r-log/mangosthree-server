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

#ifndef MANGOS_H_CHARACTERHANDLERS
#define MANGOS_H_CHARACTERHANDLERS

#include <memory>
#include <string>
#include <vector>
#include "Platform/Define.h"
#include "SessionId.h"

class ObjectGuid;
class SqlQueryHolder;
class WorldPacket;
class WorldSession;
struct CharacterEnumRow;

/// The client's character screen and character opcodes: the character list, a character created
/// or deleted, a faction set at war, watched or inactive, the tutorial flags, and the helm and the
/// cloak shown or hidden. Create and delete answer through continuations, which re-find the session
/// by account id and session id once their reads have answered. Static entry points the opcode table
/// binds, each borrowing the session for one call and storing nothing.
struct CharacterHandlers
{
    public:
        /// Everything CMSG_CHAR_CREATE carries.
        struct CharCreateRequest
        {
            std::string name;
            uint8 race = 0;
            uint8 playerClass = 0;
            uint8 gender = 0;
            uint8 skin = 0;
            uint8 face = 0;
            uint8 hairStyle = 0;
            uint8 hairColor = 0;
            uint8 facialHair = 0;
            uint8 outfitId = 0;
        };

        static void HandleCharEnum(WorldSession& session, WorldPacket& recv_data);
        static void HandleCharCreate(WorldSession& session, WorldPacket& recv_data);
        static void HandleCharDelete(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetFactionAtWar(WorldSession& session, WorldPacket& recv_data);
        static void HandleTutorialFlag(WorldSession& session, WorldPacket& recv_data);
        static void HandleTutorialClear(WorldSession& session, WorldPacket& recv_data);
        static void HandleTutorialReset(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetWatchedFaction(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetFactionInactive(WorldSession& session, WorldPacket& recv_data);
        static void HandleShowingHelm(WorldSession& session, WorldPacket& recv_data);
        static void HandleShowingCloak(WorldSession& session, WorldPacket& recv_data);
        static void QueueCharCreateAccountRead(uint32 accountId, proto::SessionId sessionId,
                                               CharCreateRequest request);
        static void QueueCharCreateRealmReads(uint32 accountId, proto::SessionId sessionId,
                                              CharCreateRequest request);
        static void QueueCharDeleteReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid);

    private:
        static void HandleCharEnumCallback(std::vector<CharacterEnumRow> const& rows, uint32 account, proto::SessionId sessionId);
        static uint8 CharCreateChecksInMemory(WorldSession* session, CharCreateRequest& request);
        static void HandleCharCreateAccountCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                    proto::SessionId sessionId, CharCreateRequest request);
        static void HandleCharCreateCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                             proto::SessionId sessionId, CharCreateRequest request);
        static void HandleCharDeleteCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                             proto::SessionId sessionId, ObjectGuid guid,
                                             uint32 charDeleteMethod);
};

#endif
