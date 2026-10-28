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

#ifndef MANGOS_H_CHATPACKETBUILDER
#define MANGOS_H_CHATPACKETBUILDER

#include "SharedDefines.h"
#include "ObjectGuid.h"

class WorldPacket;

/**
 * @file ChatPacketBuilder.h
 * @brief The chat message packet for the code below the session layer, which may not include the
 * session's builder.
 *
 * World::SetInitialWorldSettings installs the session's BuildMessageChatPacket
 * (session/packets/chat/MessageChatPacket.h) before anything is loaded, and BuildChatPacket calls
 * it. Building with none installed fails an assertion.
 */

/// A builder of one chat message packet; the parameters are BuildMessageChatPacket's.
typedef void (*ChatPacketBuilder)(WorldPacket& data, ChatMsg msgtype, char const* message, Language language, ChatTagFlags chatTag,
                                  ObjectGuid const& senderGuid, char const* senderName,
                                  ObjectGuid const& targetGuid, char const* targetName,
                                  char const* channelName, uint32 achievementId, const char* addonPrefix);

/// Installs `builder` and answers the one it replaces (NULL for none). Installed once at
/// start-up, before the map threads run.
ChatPacketBuilder InstallChatPacketBuilder(ChatPacketBuilder builder);

/// Builds one chat message into `data` with the installed builder.
void BuildChatPacket(
    WorldPacket& data, ChatMsg msgtype, char const* message, Language language = LANG_UNIVERSAL, ChatTagFlags chatTag = CHAT_TAG_NONE,
    ObjectGuid const& senderGuid = ObjectGuid(), char const* senderName = NULL,
    ObjectGuid const& targetGuid = ObjectGuid(), char const* targetName = NULL,
    char const* channelName = NULL, uint32 achievementId = 0, const char* addonPrefix = NULL);

#endif
