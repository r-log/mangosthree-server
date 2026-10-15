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
 * @brief Guild and arena charter opcode handlers
 *
 * Charters require a certain number of signatures before they can be
 * turned in to create a guild or arena team.
 *
 * None of these handlers waits on the database. Each one does
 * whatever it can decide from memory, queues its reads (AsyncPQuery, or one SqlQueryHolder
 * where a handler needs several independent rows), and answers from a continuation that
 * runs on the world thread out of UpdateResultQueue() a tick later. The petition UI waits
 * for the reply packet, so a tick of delay is invisible to it.
 *
 * A continuation cannot hold a pointer across that tick: the session may have gone and the
 * account logged in again, and the player may have been swapped for another character. So
 * every one of them captures identities (account id, session id, player guid, the request's
 * values) and re-finds both through WorldSession::FindRequesterPlayer, doing nothing at all if either
 * has changed.
 */

#include "Platform/Define.h"
#include <memory>
#include <string>
#include <sstream>
#include "Database/DatabaseEnv.h"
#include "Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "ArenaTeam.h"
#include "GossipDef.h"
#include "SocialMgr.h"
#include "PlayerRegistry.h"

/*enum PetitionType // dbc data
{
    PETITION_TYPE_GUILD      = 1,
    PETITION_TYPE_ARENA_TEAM = 3
};*/

// Charters ID in item_template
#define GUILD_CHARTER               5863
#define CHARTER_DISPLAY_ID          16161

/**
 * @brief Handles charter purchase and petition creation.
 *
 * @param recv_data The incoming petition-buy packet.
 */
void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_BUY");
    recv_data.hexlike();

    ObjectGuid guidNPC;
    std::string name;

    recv_data >> guidNPC;                                   // NPC GUID
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint64>();                          // 0
    recv_data >> name;                                      // name
    recv_data.read_skip<std::string>();                     // some string
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint16>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0
    recv_data.read_skip<uint32>();                          // 0

    for (int i = 0; i < 10; ++i)
    {
        recv_data.read_skip<std::string>();
    }

    recv_data.read_skip<uint32>();                          // client index
    recv_data.read_skip<uint32>();                          // 0

    DEBUG_LOG("Petitioner %s tried sell petition: name %s", guidNPC.GetString().c_str(), name.c_str());

    // prevent cheating
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", guidNPC.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->IsFeigningDeath())
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (!pCreature->IsTabardDesigner())
    {
        sLog.outError("WORLD: HandlePetitionBuyOpcode - unsupported npc type, npc: %s", guidNPC.GetString().c_str());
        return;
    }

    // Every check below reads memory only, so it stays here: a request that cannot succeed
    // never reaches the database at all. Each of them runs again in the continuation (C3),
    // because a tick passes before the charter is paid for.
    if (sGuildMgr.GetGuildByName(name))
    {
        SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(GUILD_CHARTER);
    if (!pProto)
    {
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, NULL, GUILD_CHARTER, 0);
        return;
    }

    if (_player->GetMoney() < sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST))
    {
        // player hasn't got enough money
        _player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, GUILD_CHARTER, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, GUILD_CHARTER, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, NULL, NULL, GUILD_CHARTER);
        return;
    }

    // C3: the read is queued BEFORE anything is charged or created. The old order paid for
    // the charter and put it in the bag first, so a continuation that never arrives -- a
    // logout in the same tick -- could leave a paid charter with no petition row behind it.
    QueuePetitionBuyRead(GetAccountId(), GetSessionId(), _player->GetObjectGuid(), guidNPC, name);
}

/**
 * @brief Stages the buy handler's only read (D7b, C3).
 *
 * Split out of the handler so it can be driven on its own: everything it needs is in its
 * arguments, so a test can queue exactly what a real request queues without a Player.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The buyer.
 * @param npcGuid The petitioner NPC the buyer is talking to.
 * @param name The requested charter name.
 */
void WorldSession::QueuePetitionBuyRead(uint32 accountId, proto::SessionId sessionId,
                                        ObjectGuid playerGuid, ObjectGuid npcGuid, std::string name)
{
    // a petition is invalid, if both the owner and the type matches
    // we checked above, if this player is in an arenateam, so this must be data corruption
    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid, npcGuid, name](QueryResult* result)
                                  {
                                      WorldSession::HandlePetitionBuyCallback(std::unique_ptr<QueryResult>(result),
                                                                              accountId, sessionId, playerGuid,
                                                                              npcGuid, name);
                                  },
                                  "SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '%u'", playerGuid.GetCounter());
}

/**
 * @brief Charges for the charter and writes the petition, once the owner's rows are known.
 *
 * @param result The owner's existing petitions.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The buyer.
 * @param npcGuid The petitioner NPC.
 * @param name The requested charter name.
 */
void WorldSession::HandlePetitionBuyCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                             proto::SessionId sessionId, ObjectGuid playerGuid,
                                             ObjectGuid npcGuid, std::string name)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    // Everything the handler checked before it queued, checked again: a tick has passed, so
    // the NPC may be out of range, the name may have been taken, the bag may have filled and
    // the money may have been spent. Only then is anything charged or created.
    Creature* pCreature = player->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    if (!pCreature->IsTabardDesigner())
    {
        sLog.outError("WORLD: HandlePetitionBuyOpcode - unsupported npc type, npc: %s", npcGuid.GetString().c_str());
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(GUILD_CHARTER);
    if (!pProto)
    {
        player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, NULL, GUILD_CHARTER, 0);
        return;
    }

    if (player->GetMoney() < sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST))
    {
        // player hasn't got enough money
        player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, GUILD_CHARTER, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, GUILD_CHARTER, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        player->SendEquipError(msg, NULL, NULL, GUILD_CHARTER);
        return;
    }

    player->ModifyMoney(-int64(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)));
    Item* charter = player->StoreNewItem(dest, GUILD_CHARTER, true);
    if (!charter)
    {
        return;
    }

    charter->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1, charter->GetGUIDLow());
    // ITEM_FIELD_ENCHANTMENT_1_1 is guild/arenateam id
    // ITEM_FIELD_ENCHANTMENT_1_1+1 is current signatures count (showed on item)
    charter->SetState(ITEM_CHANGED, player);
    player->SendNewItem(charter, 1, true, false);

    std::ostringstream ssInvalidPetitionGUIDs;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ssInvalidPetitionGUIDs << "'" << fields[0].GetUInt32() << "' , ";
        }
        while (result->NextRow());
    }

    // delete petitions with the same guid as this one
    ssInvalidPetitionGUIDs << "'" << charter->GetGUIDLow() << "'";

    DEBUG_LOG("Invalid petition GUIDs: %s", ssInvalidPetitionGUIDs.str().c_str());

    // C5: the name is never escaped on the world thread. It is bound as a parameter, and
    // whatever the connection has to do with it -- a real bind, or the escaping the plain
    // fallback does -- happens inside the delay thread's own operation. The two deletes
    // carry nothing but numbers, so they stay printf statements.
    static SqlStatementID insertPetition;
    SqlStatement stmt = CharacterDatabase.CreateStatement(insertPetition,
                        "INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES (?, ?, ?)");

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` IN ( %s )",  ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` IN ( %s )", ssInvalidPetitionGUIDs.str().c_str());
    stmt.PExecute(player->GetGUIDLow(), charter->GetGUIDLow(), name.c_str());
    CharacterDatabase.CommitTransaction();
}

/**
 * @brief Handles a petition query request.
 *
 * @param recv_data The incoming petition query packet.
 */
void WorldSession::HandlePetitionQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");
    // recv_data.hexlike();

    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >> guildguid;                                 // in mangos always same as GUID_LOPART(petitionguid)
    recv_data >> petitionguid;                              // petition guid
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", petitionguid.GetString().c_str(), guildguid);

    SendPetitionQueryOpcode(petitionguid);
}

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

    CharacterDatabase.AsyncPQuery([accountId, sessionId, petitionguid](QueryResult* result)
                                  {
                                      WorldSession::SendPetitionQueryCallback(std::unique_ptr<QueryResult>(result),
                                                                              accountId, sessionId, petitionguid);
                                  },
                                  "SELECT `ownerguid`, `name`, "
                                  "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                                  "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);
}

/**
 * @brief Builds SMSG_PETITION_QUERY_RESPONSE from the petition row.
 *
 * @param result The petition row.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param petitionguid The petition guid.
 */
void WorldSession::SendPetitionQueryCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
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

    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name      = fields[1].GetCppString();
        signs     = fields[2].GetUInt8();
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
 * @brief Handles petition renaming and updates persistent storage.
 *
 * @param recv_data The incoming petition rename packet.
 */
void WorldSession::HandlePetitionRenameOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_RENAME");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    std::string newname;

    recv_data >> petitionGuid;                              // guid
    recv_data >> newname;                                   // new name

    Item* item = _player->GetInventoryMgr().GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    // C3: read first, then escape and write in the continuation.
    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid playerGuid = _player->GetObjectGuid();

    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid, petitionGuid, newname](QueryResult* result)
                                  {
                                      WorldSession::HandlePetitionRenameCallback(std::unique_ptr<QueryResult>(result),
                                                                                 accountId, sessionId, playerGuid,
                                                                                 petitionGuid, newname);
                                  },
                                  "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
}

/**
 * @brief Renames the petition once it is known to exist.
 *
 * @param result The petition existence row.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The requesting player.
 * @param petitionGuid The petition guid.
 * @param newname The requested name.
 */
void WorldSession::HandlePetitionRenameCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                                proto::SessionId sessionId, ObjectGuid playerGuid,
                                                ObjectGuid petitionGuid, std::string newname)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    if (!result)
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition: %s", petitionGuid.GetString().c_str());
        return;
    }

    // The charter has to still be the player's: a tick has passed since the handler looked.
    if (!player->GetInventoryMgr().GetItemByGuid(petitionGuid))
    {
        return;
    }

    if (sGuildMgr.GetGuildByName(newname))
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(newname) || !ObjectMgr::IsValidCharterName(newname))
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_INVALID);
        return;
    }

    // C5: bound, not escaped here -- see HandlePetitionBuyCallback.
    static SqlStatementID renamePetition;
    SqlStatement stmt = CharacterDatabase.CreateStatement(renamePetition,
                        "UPDATE `petition` SET `name` = ? WHERE `petitionguid` = ?");
    stmt.PExecute(newname.c_str(), petitionGuid.GetCounter());

    DEBUG_LOG("Petition %s renamed to '%s'", petitionGuid.GetString().c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, 8 + newname.size() + 1);
    data << petitionGuid;
    data << newname;
    session->SendPacket(&data);
}

/**
 * @brief Handles declining a petition offer and notifies the owner.
 *
 * @param recv_data The incoming petition decline packet.
 */
void WorldSession::HandlePetitionDeclineOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_DECLINE");  // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    recv_data >> petitionGuid;                              // petition guid

    DEBUG_LOG("Petition %s declined by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    ObjectGuid playerGuid = _player->GetObjectGuid();

    CharacterDatabase.AsyncPQuery([playerGuid](QueryResult* result)
                                  {
                                      WorldSession::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult>(result),
                                                                                  playerGuid);
                                  },
                                  "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
}

/**
 * @brief Tells the petition owner that the offer was declined.
 *
 * The one packet this continuation sends goes to the OWNER, not to the requester, and it
 * names the decliner by GUID -- so it is built from the captured guid and the requester is
 * neither re-found nor needed (D7f). Dropping it because the decliner logged out in the
 * intervening tick would leave the owner's charter UI waiting for an answer that has
 * already been given; before the read was queued at all, the owner always got it.
 *
 * @param result The petition's owner row.
 * @param playerGuid The declining player.
 */
void WorldSession::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult> result, ObjectGuid playerGuid)
{
    if (!result)
    {
        return;
    }

    Field* fields = result->Fetch();
    ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());

    if (Player* owner = sPlayerRegistry.Find(ownerguid))    // petition owner online
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << playerGuid;
        owner->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Handles a request to show the petitioner vendor list.
 *
 * @param recv_data The incoming show-list packet.
 */
void WorldSession::HandlePetitionShowListOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");
    // recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    SendPetitionShowList(guid);
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
        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
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
