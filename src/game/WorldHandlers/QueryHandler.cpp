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
 * @file QueryHandler.cpp
 * @brief The query answers the query handlers send: a player's name, from the player online or
 * from the character database, and the server time.
 */

#include "Platform/Define.h"
#include <string>
#include <ctime>
#include "data/Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "data/ResetSchedule.h"
#include "data/GameConfig.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "persistence/characters/CharacterRepository.h"

/**
 * @brief Sends an in-memory name query response for a player.
 *
 * @param p The player being queried.
 */
void WorldSession::SendNameQueryOpcode(Player* p)
{
    if (!p)
    {
        return;
    }

    // guess size
    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + 1 + 1 + 1 + 1 + 1 + 10));
    data << p->GetPackGUID();                               // player guid
    data << uint8(0);                                       // added in 3.1; if > 1, then end of packet
    data << p->GetName();                                   // played name
    data << uint8(0);                                       // realm name for cross realm BG usage
    data << uint8(p->getRace());
    data << uint8(p->getGender());
    data << uint8(p->getClass());
    if (DeclinedName const* names = p->GetDeclinedNames())
    {
        data << uint8(1);                                   // is declined
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            data << names->name[i];
        }
    }
    else
    {
        data << uint8(0);                                   // is not declined
    }

    SendPacket(&data);
}

/**
 * @brief Starts an asynchronous database lookup for a player name query.
 *
 * @param guid The queried player guid.
 */
void WorldSession::SendNameQueryOpcodeFromDB(ObjectGuid guid)
{
    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    CharacterRepository::QueryName(guid.GetCounter(), sGameConfig.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED),
                                   [accountId, sessionId](std::optional<CharacterNameRow> row)
                                   {
                                       WorldSession::SendNameQueryOpcodeFromDBCallBack(row, accountId, sessionId);
                                   });
}

/**
 * @brief Completes an asynchronous player name query from the database.
 *
 * @param row The queried character, none when it does not exist.
 * @param accountId The requesting account id.
 * @param sessionId The session the request arrived on.
 */
void WorldSession::SendNameQueryOpcodeFromDBCallBack(std::optional<CharacterNameRow> const& row, uint32 accountId, proto::SessionId sessionId)
{
    if (!row)
    {
        return;
    }

    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session)
    {
        return;
    }

    uint32 lowguid      = row->guid;
    std::string name = row->name;
    uint8 pRace = 0, pGender = 0, pClass = 0;
    if (name.empty())
    {
        name         = session->GetMangosString(LANG_NON_EXIST_CHARACTER);
    }
    else
    {
        pRace        = row->race;
        pGender      = row->gender;
        pClass       = row->classId;
    }

    // guess size
    WorldPacket data(SMSG_NAME_QUERY_RESPONSE, (8 + 1 + 1 + 1 + 1 + 1 + 1 + 10));
    data << ObjectGuid(HIGHGUID_PLAYER, lowguid).WriteAsPacked();
    data << uint8(0);                                       // added in 3.1; if > 1, then end of packet
    data << name;
    data << uint8(0);                                       // realm name for cross realm BG usage
    data << uint8(pRace);                                   // race
    data << uint8(pGender);                                 // gender
    data << uint8(pClass);                                  // class

    // if the first declined name is empty, the rest must be too
    if (sGameConfig.getConfig(CONFIG_BOOL_DECLINED_NAMES_USED) && !row->declinedNames[0].empty())
    {
        data << uint8(1);                                   // is declined
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            data << row->declinedNames[i];
        }
    }
    else
    {
        data << uint8(0);                                   // is not declined
    }

    session->SendPacket(&data);
}

/**
 * @brief Sends the current server time to the client.
 */
void WorldSession::SendQueryTimeResponse()
{
    WorldPacket data(SMSG_QUERY_TIME_RESPONSE, 4 + 4);
    data << uint32(time(NULL));
    data << uint32(sResetSchedule.nextDailyQuestReset - time(NULL));
    SendPacket(&data);
}
