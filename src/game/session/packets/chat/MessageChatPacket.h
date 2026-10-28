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

#ifndef MANGOS_H_MESSAGECHATPACKET
#define MANGOS_H_MESSAGECHATPACKET

#include "SharedDefines.h"
#include "ObjectGuid.h"

class WorldPacket;

/**
 * @file MessageChatPacket.h
 * @brief SMSG_MESSAGECHAT, or SMSG_GM_MESSAGECHAT for a message tagged CHAT_TAG_GM: one chat
 * message of any type.
 *
 * The type and the language, the sender's guid and a zero uint32, then by type: the sender's name
 * and the target for a monster's, a raid boss's, a Battle.net or a foreign whisper's message (the
 * target's name too when the target is a creature), the target for a battleground system
 * message (its name when it is not a player), the target for an achievement; otherwise the GM's
 * name when tagged GM, then the channel name and the target, the addon prefix, or the target.
 * Then the text's length and the text, the tag, the achievement id for an achievement, and a
 * float and a byte for a raid boss's whisper or emote. src/tests/MessageChatPacketTest.cpp pins
 * the bytes.
 */

/**
 * @brief Initializes `data` as the message's packet and writes it.
 *
 * @param data          The packet to fill.
 * @param msgtype       The message type (ChatMsg in SharedDefines.h).
 * @param message       The text.
 * @param language      The language (Language in SharedDefines.h).
 * @param chatTag       The sender's tags (PlayerChatTag in SharedDefines.h).
 * @param senderGuid    The sender; may be empty, but the client's ignore list needs it.
 * @param senderName    Required for the monster, raid boss, Battle.net and foreign whisper types, and for a GM.
 * @param targetGuid    Often empty; needed for the monster, Battle.net, battleground and achievement types.
 * @param targetName    Needed for the monster, Battle.net and battleground types.
 * @param channelName   Required only for CHAT_MSG_CHANNEL.
 * @param achievementId Required only for the achievement types.
 * @param addonPrefix   Required only for CHAT_MSG_ADDON.
 */
void BuildMessageChatPacket(
    WorldPacket& data, ChatMsg msgtype, char const* message, Language language = LANG_UNIVERSAL, ChatTagFlags chatTag = CHAT_TAG_NONE,
    ObjectGuid const& senderGuid = ObjectGuid(), char const* senderName = NULL,
    ObjectGuid const& targetGuid = ObjectGuid(), char const* targetName = NULL,
    char const* channelName = NULL, uint32 achievementId = 0, const char* addonPrefix = NULL);

#endif
