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

/**
 * @file GuildHandler.cpp
 * @brief Guild opcode handlers
 */

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include <cstring>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GossipDef.h"
#include "SocialMgr.h"
#include "Calendar.h"
#include "PlayerRegistry.h"
#include "persistence/characters/CharacterTransaction.h"

/**
 * @brief Sends the result of a guild command back to the client.
 *
 * @param typecmd The guild command type.
 * @param str The related player or guild name.
 * @param cmdresult The result code.
 */
void WorldSession::SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult)
{
    WorldPacket data(SMSG_GUILD_COMMAND_RESULT, 8 + str.size() + 1);
    data << typecmd;
    data << cmdresult;
    data.WriteBits(str.length(), 8);
    data.WriteStringData(str);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_COMMAND_RESULT)");
}

/******  GUILD BANK  *******/

/**
 * @brief Sends the result of a guild emblem save request.
 *
 * @param msg The guild emblem result code.
 */
void WorldSession::SendSaveGuildEmblem(uint32 msg)
{
    WorldPacket data(MSG_SAVE_GUILD_EMBLEM, 4);
    data << uint32(msg);                                    // not part of guild
    SendPacket(&data);
}

