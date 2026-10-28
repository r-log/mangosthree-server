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

#ifndef MANGOS_H_CHARACTERCUSTOMIZEHANDLERS
#define MANGOS_H_CHARACTERCUSTOMIZEHANDLERS

#include <memory>
#include <optional>
#include <string>
#include "Platform/Define.h"
#include "SessionId.h"

class ObjectGuid;
class SqlQueryHolder;
class WorldPacket;
class WorldSession;
struct RenameTargetRow;

/// The client's character customization and client-settings opcodes: a character renamed or
/// customized at login, its declined names, the barber's appearance change, the equipment sets
/// saved, deleted and used, the character list reordered, a currency's flags and the load screen.
/// The rename and the customize answer through continuations, which re-find the session by
/// account id and session id. Static entry points the opcode table binds, each borrowing the
/// session for one call and storing nothing.
struct CharacterCustomizeHandlers
{
    public:
        /// Everything CMSG_CHAR_CUSTOMIZE carries beyond the character guid.
        struct CharCustomizeRequest
        {
            std::string newname;
            uint8 gender = 0;
            uint8 skin = 0;
            uint8 face = 0;
            uint8 hairStyle = 0;
            uint8 hairColor = 0;
            uint8 facialHair = 0;
        };

        static void HandleCharRename(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetPlayerDeclinedNames(WorldSession& session, WorldPacket& recv_data);
        static void HandleAlterAppearance(WorldSession& session, WorldPacket& recv_data);
        static void HandleCharCustomize(WorldSession& session, WorldPacket& recv_data);
        static void HandleEquipmentSetSave(WorldSession& session, WorldPacket& recv_data);
        static void HandleEquipmentSetDelete(WorldSession& session, WorldPacket& recv_data);
        static void HandleEquipmentSetUse(WorldSession& session, WorldPacket& recv_data);
        static void HandleReorderCharacters(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetCurrencyFlags(WorldSession& session, WorldPacket& recv_data);
        static void HandleLoadScreen(WorldSession& session, WorldPacket& recv_data);
        static void QueueCharCustomizeReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid,
                                            CharCustomizeRequest request);

    private:
        static void HandleChangePlayerNameCallback(std::optional<RenameTargetRow> const& row, uint32 accountId,
                                                   proto::SessionId sessionId, std::string newname);
        static void HandleCharCustomizeCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                proto::SessionId sessionId, ObjectGuid guid,
                                                CharCustomizeRequest request);
};

#endif
