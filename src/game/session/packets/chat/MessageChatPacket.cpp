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

#include "session/packets/chat/MessageChatPacket.h"

#include <cstring>
#include "Utilities/Errors.h"
#include "WorldPacket.h"
#include "Opcodes.h"

void BuildMessageChatPacket(WorldPacket& data, ChatMsg msgtype, char const* message, Language language /*= LANG_UNIVERSAL*/, ChatTagFlags chatTag /*= CHAT_TAG_NONE*/,
                            ObjectGuid const& senderGuid /*= ObjectGuid()*/, char const* senderName /*= NULL*/,
                            ObjectGuid const& targetGuid /*= ObjectGuid()*/, char const* targetName /*= NULL*/,
                            char const* channelName /*= NULL*/, uint32 achievementId /*= 0*/, const char* addonPrefix /*= NULL*/)
{
    bool isGM = chatTag & CHAT_TAG_GM;
    bool isAchievement = false;

    data.Initialize(isGM ? SMSG_GM_MESSAGECHAT : SMSG_MESSAGECHAT);
    data << uint8(msgtype);
    data << uint32(language);
    data << ObjectGuid(senderGuid);
    data << uint32(0);                                              // 2.1.0

    switch (msgtype)
    {
        case CHAT_MSG_MONSTER_SAY:
        case CHAT_MSG_MONSTER_PARTY:
        case CHAT_MSG_MONSTER_YELL:
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_MONSTER_EMOTE:
        case CHAT_MSG_RAID_BOSS_WHISPER:
        case CHAT_MSG_RAID_BOSS_EMOTE:
        case CHAT_MSG_BATTLENET:
        case CHAT_MSG_WHISPER_FOREIGN:
            MANGOS_ASSERT(senderName);
            data << uint32(strlen(senderName) + 1);
            data << senderName;
            data << ObjectGuid(targetGuid);                         // Unit Target
            if (targetGuid && !targetGuid.IsPlayer() && !targetGuid.IsPet() && (msgtype != CHAT_MSG_WHISPER_FOREIGN))
            {
                data << uint32(strlen(targetName) + 1);             // target name length
                data << targetName;                                 // target name
            }
            break;
        case CHAT_MSG_BG_SYSTEM_NEUTRAL:
        case CHAT_MSG_BG_SYSTEM_ALLIANCE:
        case CHAT_MSG_BG_SYSTEM_HORDE:
            data << ObjectGuid(targetGuid);                         // Unit Target
            if (targetGuid && !targetGuid.IsPlayer())
            {
                MANGOS_ASSERT(targetName);
                data << uint32(strlen(targetName) + 1);             // target name length
                data << targetName;                                 // target name
            }
            break;
        case CHAT_MSG_ACHIEVEMENT:
        case CHAT_MSG_GUILD_ACHIEVEMENT:
            data << ObjectGuid(targetGuid);                         // Unit Target
            isAchievement = true;
            break;
        default:
            if (isGM)
            {
                MANGOS_ASSERT(senderName);
                data << uint32(strlen(senderName) + 1);
                data << senderName;
            }

            if (msgtype == CHAT_MSG_CHANNEL)
            {
                MANGOS_ASSERT(channelName);
                data << channelName;
                data << ObjectGuid(targetGuid);
            }
            else if (msgtype == CHAT_MSG_ADDON)
            {
                MANGOS_ASSERT(addonPrefix);
                data << addonPrefix;
            }
            else
            {
                data << ObjectGuid(targetGuid);
            }
            break;
    }
    MANGOS_ASSERT(message);
    data << uint32(strlen(message) + 1);
    data << message;
    data << uint8(chatTag);

    if (isAchievement)
    {
        data << uint32(achievementId);
    }

    if (msgtype == CHAT_MSG_RAID_BOSS_WHISPER || msgtype == CHAT_MSG_RAID_BOSS_EMOTE)
    {
        data << float(0.0f);                                    // Added in 4.2.0, unk
        data << uint8(0);                                       // Added in 4.2.0, unk
    }
}
