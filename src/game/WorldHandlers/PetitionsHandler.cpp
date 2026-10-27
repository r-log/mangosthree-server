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
 * @file PetitionsHandler.cpp
 * @brief Defines three WorldSession members: SendPetitionQueryOpcode, which queues the read a petition
 * query answers, SendPetitionQueryCallback, which builds that answer from the row a tick later on the
 * world thread and sends it to the session the query came from, and SendPetitionShowList, which sends
 * the client a petitioner's charter list.
 */

#include "Platform/Define.h"
#include <memory>
#include <string>
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "persistence/characters/social/PetitionRepository.h"

/*enum PetitionType // dbc data
{
    PETITION_TYPE_GUILD      = 1,
    PETITION_TYPE_ARENA_TEAM = 3
};*/

// Charters ID in item_template
#define GUILD_CHARTER               5863
#define CHARTER_DISPLAY_ID          16161

/**
 * @brief Sends petition metadata for a specific petition item.
 *
 * @param petitionguid The petition guid.
 */
void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    uint32 petitionLowGuid = petitionguid.GetCounter();

    // This reply is about the petition, not about the player -- it touches no Player at
    // all -- so the identity that has to survive the round trip is the session's alone.
    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();

    PetitionRepository::QueryDetails(petitionLowGuid,
                                     [accountId, sessionId, petitionguid](std::optional<PetitionDetailsRow> row)
                                     {
                                         WorldSession::SendPetitionQueryCallback(row, accountId, sessionId, petitionguid);
                                     });
}

/**
 * @brief Builds SMSG_PETITION_QUERY_RESPONSE from the petition row.
 *
 * @param row The petition, none when it does not exist.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param petitionguid The petition guid.
 */
void WorldSession::SendPetitionQueryCallback(std::optional<PetitionDetailsRow> const& row, uint32 accountId,
                                             proto::SessionId sessionId, ObjectGuid petitionguid)
{
    WorldSession* session = sWorld.FindSession(accountId);
    if (!session || session->GetSessionId() != sessionId)
    {
        return;
    }

    uint32 petitionLowGuid = petitionguid.GetCounter();

    ObjectGuid ownerGuid;
    std::string name = "NO_NAME_FOR_GUID";
    uint8 signs = 0;

    if (row)
    {
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, row->ownerGuid);
        name      = row->name;
        signs     = row->signs;
    }
    else
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition (GUID: %u)", petitionLowGuid);
        return;
    }

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE, (4 + 8 + name.size() + 1 + 1 + 4 * 12 + 2 + 10));
    data << uint32(petitionLowGuid);                        // guild/team guid (in mangos always same as GUID_LOPART(petition guid)
    data << ObjectGuid(ownerGuid);                          // charter owner guid
    data << name;                                           // name (guild/arena team)
    data << uint8(0);                                       // some string
    data << uint32(4);
    data << uint32(4);
    data << uint32(0);                                      // bypass client - side limitation, a different value is needed here for each petition
    data << uint32(0);                                      // 5
    data << uint32(0);                                      // 6
    data << uint32(0);                                      // 7
    data << uint32(0);                                      // 8
    data << uint16(0);                                      // 9 2 bytes field
    data << uint32(0);                                      // 10
    data << uint32(0);                                      // 11
    data << uint32(0);                                      // 13 count of next strings?

    for (int i = 0; i < 10; ++i)
    {
        data << uint8(0);                                   // some string
    }

    data << uint32(0);                                      // 14
    data << uint32(0);                                      // 15 0 - guild, 1 - arena team

    session->SendPacket(&data);
}

/**
 * @brief Sends the available petition list from a petitioner NPC.
 *
 * @param guid The petitioner NPC guid.
 */
void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionShowList - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->IsFeigningDeath())
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 6);
    data << guid;                           // npc guid

    if (pCreature->IsTabardDesigner())
    {
        data << uint8(1);                   // count
        data << uint32(1);                  // index
        data << uint32(GUILD_CHARTER);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID); // charter display id
        data << uint32(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)); // charter cost
        data << uint32(0);                  // unknown
        data << uint32(sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS));  // required signs
    }

    SendPacket(&data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
