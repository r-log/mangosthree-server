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

#include <string>
#include "Player.h"
#include "PlayerRegistry.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "GMTicketMgr.h"
#include "Log.h"
#include "data/GameConfig.h"
#include "WorldPacket.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Util.h"
#include "data/ChatPacketBuilder.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"
#include "persistence/characters/social/TicketRepository.h"

#include <cmath>

/**
 * @brief Sends a say chat message from the player to nearby listeners.
 *
 * @param text The message text.
 * @param language The language used for the chat packet.
 */
void Player::Say(const std::string& text, const uint32 language)
{
    WorldPacket data;
    BuildChatPacket(data, CHAT_MSG_SAY, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

/**
 * @brief Sends a yell chat message from the player to nearby listeners.
 *
 * @param text The message text.
 * @param language The language used for the chat packet.
 */
void Player::Yell(const std::string& text, const uint32 language)
{
    WorldPacket data;
    BuildChatPacket(data, CHAT_MSG_YELL, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

/**
 * @brief Sends a text emote message from the player to nearby listeners.
 *
 * @param text The emote text.
 */
void Player::TextEmote(const std::string& text)
{
    WorldPacket data;
    BuildChatPacket(data, CHAT_MSG_EMOTE, text.c_str(), LANG_UNIVERSAL, GetChatTag(), GetObjectGuid(), GetName());
    SendMessageToSetInRange(&data, sGameConfig.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), true, !sGameConfig.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT));
}

/**
 * @brief Logs a whisper to the database when whisper logging is enabled.
 *
 * @param text The whisper text.
 * @param receiver The recipient player GUID.
 */
void Player::LogWhisper(const std::string& text, ObjectGuid receiver)
{
    WhisperLoggingLevels loggingLevel = WhisperLoggingLevels(sGameConfig.getConfig(CONFIG_UINT32_LOG_WHISPERS));

    if (loggingLevel == WHISPER_LOGGING_NONE)
    {
        return;
    }

    //Try to find ticket by either this player or the receiver
    GMTicket* ticket = sTicketMgr.GetGMTicket(GetObjectGuid());
    if (!ticket)
    {
        ticket = sTicketMgr.GetGMTicket(receiver);
    }

    uint32 ticketId = 0;
    if (ticket)
    {
        ticketId = ticket->GetId();
    }

    bool isSomeoneGM = false;

    //Find out if at least one of them is a GM for ticket logging
    if (GetSession()->GetSecurity() >= SEC_GAMEMASTER)
    {
        isSomeoneGM = true;
    }
    else
    {
        Player* pRecvPlayer = sPlayerRegistry.Find(receiver);
        if (pRecvPlayer && pRecvPlayer->GetSession()->GetSecurity() >= SEC_GAMEMASTER)
        {
            isSomeoneGM = true;
        }
    }

    if ((loggingLevel == WHISPER_LOGGING_TICKETS && ticket && isSomeoneGM)
        || loggingLevel == WHISPER_LOGGING_EVERYTHING)
    {
        TicketRepository::InsertWhisper(receiver.GetCounter(), GetObjectGuid().GetCounter(), text, ticketId);
    }
}

/**
 * @brief Sends a whisper to another player and handles local response messages.
 *
 * @param text The whisper text.
 * @param language The requested chat language.
 * @param receiver The recipient player GUID.
 */
void Player::Whisper(const std::string& text, uint32 language, ObjectGuid receiver)
{
    Player* rPlayer = sPlayerRegistry.Find(receiver);

    WorldPacket data;
    BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), GetChatTag(), GetObjectGuid(), GetName());
    rPlayer->GetSession()->SendPacket(&data);

    // do not send confirmations, afk, dnd or system notifications for addon messages
    if (language == LANG_ADDON)
    {
        data.clear();
        BuildChatPacket(data, CHAT_MSG_WHISPER, text.c_str(), Language(language), CHAT_TAG_NONE, rPlayer->GetObjectGuid());
        LogWhisper(text, receiver);
        GetSession()->SendPacket(&data);
    }

    data.clear();
    BuildChatPacket(data, CHAT_MSG_WHISPER_INFORM, text.c_str(), Language(language), CHAT_TAG_NONE, rPlayer->GetObjectGuid());
    GetSession()->SendPacket(&data);

    if (!isAcceptWhispers())
    {
        SetAcceptWhispers(true);
        ReportSystemMessage(LANG_COMMAND_WHISPERON);
    }

    // announce afk or dnd message
    if (rPlayer->isAFK() || rPlayer->isDND())
    {
        const ChatMsg msgtype = rPlayer->isAFK() ? CHAT_MSG_AFK : CHAT_MSG_DND;
        data.clear();
        BuildChatPacket(data, msgtype, rPlayer->autoReplyMsg.c_str(), LANG_UNIVERSAL, CHAT_TAG_NONE, rPlayer->GetObjectGuid());
        GetSession()->SendPacket(&data);
    }
}
