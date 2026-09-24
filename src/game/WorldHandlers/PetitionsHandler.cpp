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
 * This file handles petition-related opcodes for guild and arena charters:
 * - CMSG_PETITION_BUY: Buy guild/arena charter
 * - CMSG_PETITION_SHOW_SIGNATURES: Show charter signatures
 * - CMSG_PETITION_SIGN: Sign charter
 * - CMSG_PETITION_OFFER: Offer charter to player
 * - CMSG_PETITION_TURN_IN: Turn in completed charter
 * - CMSG_QUERY_PETITION: Query charter info
 *
 * Charters require a certain number of signatures before they can be
 * turned in to create a guild or arena team.
 *
 * Decoupling D7b: none of these handlers waits on the database any more. Each one does
 * whatever it can decide from memory, queues its reads (AsyncPQuery, or one SqlQueryHolder
 * where a handler needs several independent rows), and answers from a continuation that
 * runs on the world thread out of UpdateResultQueue() a tick later. The petition UI waits
 * for the reply packet, so a tick of delay is invisible to it.
 *
 * A continuation cannot hold a pointer across that tick: the session may have gone and the
 * account logged in again, and the player may have been swapped for another character. So
 * every one of them captures identities (account id, session id, player guid, the request's
 * values) and re-finds both through FindRequester() below, doing nothing at all if either
 * has changed.
 */

#include "Platform/Define.h"
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
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

namespace
{
    /// The slots of the show-signatures / offer holder: two reads that need each other's
    /// answer not at all, so they run back to back in one round trip.
    enum PetitionShowSlot
    {
        PETITION_SHOW_PETITION      = 0,                    // the petition exists at all
        PETITION_SHOW_SIGNATURES    = 1,                    // who has signed it
        PETITION_SHOW_SLOT_COUNT    = 2
    };

    /// The slots of the sign handler's FIRST holder: the petition row (owner and the current
    /// count) and the account's existing signatures. Independent, so one round trip.
    enum PetitionSignReadSlot
    {
        PETITION_SIGN_READ_PETITION = 0,
        PETITION_SIGN_READ_ACCOUNT  = 1,
        PETITION_SIGN_READ_COUNT    = 2
    };

    /// The slots of the sign handler's SECOND holder -- the atomic sign itself (D7b, C4).
    /// The three statements run back to back on the delay thread under one connection lock:
    /// what the row looked like before, the conditional insert, what it looks like after.
    /// Nothing else can interleave, so the answer says which of the two happened.
    enum PetitionSignedSlot
    {
        PETITION_SIGNED_BEFORE      = 0,
        PETITION_SIGNED_INSERT      = 1,
        PETITION_SIGNED_AFTER       = 2,
        PETITION_SIGNED_COUNT       = 3
    };

    /// The slots of the turn-in holder: the petition row and its signatures.
    enum PetitionTurnInSlot
    {
        PETITION_TURN_IN_PETITION   = 0,
        PETITION_TURN_IN_SIGNATURES = 1,
        PETITION_TURN_IN_COUNT      = 2
    };

    /**
     * @brief Re-find the session and player a queued petition request came from (D7b, C1).
     *
     * Identities, never pointers: the account id and the session id pin the session the
     * request arrived on (a reconnected account is a different session id), and the guid
     * pins the character. A player found through the session is compared against the one
     * the registry has under that guid, so a character swap on the same session is caught
     * as well.
     *
     * @return false when either is gone or has been replaced -- the continuation then does
     *         nothing, which is the same thing that happens today when a player logs out
     *         with a request in flight.
     */
    bool FindRequester(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,
                       WorldSession*& session, Player*& player)
    {
        session = sWorld.FindSession(accountId);
        if (!session || session->GetSessionId() != sessionId)
        {
            return false;
        }

        player = session->GetPlayer();
        if (!player || player != sPlayerRegistry.Find(playerGuid))
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Queue a staged holder, freeing it if the database refuses it.
     *
     * DelayQueryHolder() answers false without queueing once the database is shutting down
     * (decoupling D7a); the holder is the callback's to free, so a refusal is the caller's
     * to clean up or it leaks both the holder and its staged statements.
     */
    void QueuePetitionHolder(SqlQueryHolder* holder,
                             std::function<void(QueryResult*, SqlQueryHolder*)> callback)
    {
        if (!CharacterDatabase.DelayQueryHolder(std::move(callback), holder))
        {
            delete holder;                                  // delete all unprocessed queries
        }
    }
}

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
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
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
 * @brief Sends the current signature list for a petition.
 *
 * @param recv_data The incoming show-signatures packet.
 */
void WorldSession::HandlePetitionShowSignOpcode(WorldPacket& recv_data)
{
    // ok
    DEBUG_LOG("Received opcode CMSG_PETITION_SHOW_SIGNATURES");
    // recv_data.hexlike();

    ObjectGuid petitionguid;
    recv_data >> petitionguid;                              // petition guid

    // solve (possible) some strange compile problems with explicit use GUID_LOPART(petitionguid) at some GCC versions (wrong code optimization in compiler?)
    uint32 petitionguid_low = petitionguid.GetCounter();

    // The existence check and the signature list are keyed on the petition guid alone, so
    // neither needs the other's answer: one holder, one round trip, instead of two.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_SHOW_SLOT_COUNT);
    holder->SetPQuery(PETITION_SHOW_PETITION,
                      "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionguid_low);
    holder->SetPQuery(PETITION_SHOW_SIGNATURES,
                      "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionguid_low);

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid playerGuid = _player->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionguid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    WorldSession::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                 accountId, sessionId, playerGuid,
                                                                                 petitionguid);
                                });
}

/**
 * @brief Sends the signature list once the petition and its signers are known.
 *
 * @param holder The staged reads.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The requesting player.
 * @param petitionGuid The petition guid.
 */
void WorldSession::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                  proto::SessionId sessionId, ObjectGuid playerGuid,
                                                  ObjectGuid petitionGuid)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    std::unique_ptr<QueryResult> petition(holder->GetResult(PETITION_SHOW_PETITION));
    std::unique_ptr<QueryResult> signatures(holder->GetResult(PETITION_SHOW_SIGNATURES));

    if (!petition)
    {
        sLog.outError("any petition on server...");
        return;
    }

    // if has guild => error, return;
    if (player->GetGuildId())
    {
        return;
    }

    uint8 signs = 0;

    // result==NULL also correct in case no sign yet
    if (signatures)
    {
        signs = (uint8)signatures->GetRowCount();
    }

    DEBUG_LOG("CMSG_PETITION_SHOW_SIGNATURES petition: %s", petitionGuid.GetString().c_str());

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << petitionGuid;                                   // petition guid
    data << player->GetObjectGuid();                        // owner guid
    data << uint32(petitionGuid.GetCounter());              // guild guid (in mangos always same as GUID_LOPART(petitionguid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = signatures->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << signerGuid;                                 // Player GUID
        data << uint32(0);                                  // there 0 ...

        signatures->NextRow();
    }

    session->SendPacket(&data);
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

    Item* item = _player->GetItemByGuid(petitionGuid);
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
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    if (!result)
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition: %s", petitionGuid.GetString().c_str());
        return;
    }

    // The charter has to still be the player's: a tick has passed since the handler looked.
    if (!player->GetItemByGuid(petitionGuid))
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
 * @brief Handles signing a guild petition.
 *
 * @param recv_data The incoming petition sign packet.
 */
void WorldSession::HandlePetitionSignOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");    // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> unk;

    // The one check that needs nothing but memory stays on the tick thread (C4).
    if (_player->GetGuildId() || _player->GetGuildIdInvited())
    {
        // close at signer side
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_IN_GUILD);
        return;
    }

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    // The petition row (for the owner and the current count) and this account's existing
    // signatures: neither answer depends on the other, so both go in one holder.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_SIGN_READ_COUNT);
    holder->SetPQuery(PETITION_SIGN_READ_PETITION,
                      "SELECT `ownerguid`, "
                      "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                      "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);
    // client doesn't allow to sign petition two times by one character, but not check sign by another character from same account
    // not allow sign another player from already sign player account
    holder->SetPQuery(PETITION_SIGN_READ_ACCOUNT,
                      "SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '%u'", GetAccountId());

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid playerGuid = _player->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    WorldSession::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                             accountId, sessionId, playerGuid,
                                                                             petitionGuid);
                                });
}

/**
 * @brief Decides whether a sign request may go ahead, once the petition is known.
 *
 * Everything the old handler did between its two reads happens here, in the same order and
 * with the same replies. Only when none of them answers does the request reach the atomic
 * sign itself.
 *
 * @param holder The staged reads.
 * @param accountId The signing account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The signer.
 * @param petitionGuid The petition guid.
 */
void WorldSession::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                              proto::SessionId sessionId, ObjectGuid playerGuid,
                                              ObjectGuid petitionGuid)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    std::unique_ptr<QueryResult> petition(holder->GetResult(PETITION_SIGN_READ_PETITION));
    std::unique_ptr<QueryResult> accountSigns(holder->GetResult(PETITION_SIGN_READ_ACCOUNT));

    if (!petition)
    {
        sLog.outError("any petition on server...");
        return;
    }

    Field* fields = petition->Fetch();
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    uint8 signs = fields[1].GetUInt8();

    if (ownerGuid == player->GetObjectGuid())
    {
        return;
    }

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
            player->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(ownerGuid))
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    /* todo: this needs to be handled properly */
    if (++signs > sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS))
    {
        // close at signer side
        player->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_PETITION_FULL);
        return;
    }

    if (accountSigns)
    {
        fields = accountSigns->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        uint32 otherPetition = fields[1].GetUInt32();
        if (otherPetition == petitionGuid.GetCounter())
        {
            // close at signer side
            player->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_ALREADY_SIGNED);

            // update for owner if online
            if (Player* owner = sPlayerRegistry.Find(ownerGuid))
            {
                owner->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_ALREADY_SIGNED);
            }
            return;
        }
        else if (signerGuid == player->GetObjectGuid())
        {
            // close at signer side
            player->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_ALREADY_SIGNED_OTHER);

            // update for owner if online
            if (Player* owner = sPlayerRegistry.Find(ownerGuid))
            {
                owner->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_ALREADY_SIGNED_OTHER);
            }
            return;
        }
    }

    QueuePetitionSignHolder(accountId, sessionId, playerGuid, petitionGuid, ownerGuid,
                            sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS));
}

/**
 * @brief Stages the atomic sign: one holder, three statements (D7b, C4).
 *
 * The old handler counted the signatures, decided, and inserted in three separate steps
 * with the world thread waiting between them -- two clients signing in the same tick both
 * saw room and both were inserted, and a signature that was already there could not be told
 * apart from one this request had just added. Here the three statements run back to back on
 * the delay thread, under the connection's lock, and the insert itself carries the rules:
 * the petition must exist, the count must be under the limit, and this player must not
 * already be on it. Comparing the row before with the row after is what names the outcome.
 *
 * Split out of the continuation so it can be driven on its own, with no Player and no
 * configuration: the sign limit is a parameter, not a lookup.
 *
 * @param accountId The signing account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The signer.
 * @param petitionGuid The petition guid.
 * @param ownerGuid The petition owner, for the copy of the reply.
 * @param maxSigns The number of signatures a petition may hold.
 */
void WorldSession::QueuePetitionSignHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,
                                           ObjectGuid petitionGuid, ObjectGuid ownerGuid, uint32 maxSigns)
{
    uint32 petitionLowGuid = petitionGuid.GetCounter();
    uint32 playerLowGuid = playerGuid.GetCounter();

    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_SIGNED_COUNT);
    holder->SetPQuery(PETITION_SIGNED_BEFORE,
                      "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u'",
                      petitionLowGuid, playerLowGuid);
    holder->SetPQuery(PETITION_SIGNED_INSERT,
                      "INSERT INTO `petition_sign` (`ownerguid`, `petitionguid`, `playerguid`, `player_account`) "
                      "SELECT `ownerguid`, `petitionguid`, '%u', '%u' FROM `petition` "
                      "WHERE `petitionguid` = '%u' "
                      "AND (SELECT COUNT(*) FROM `petition_sign` WHERE `petitionguid` = '%u') < %u "
                      "AND NOT EXISTS (SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u')",
                      playerLowGuid, accountId, petitionLowGuid, petitionLowGuid, maxSigns,
                      petitionLowGuid, playerLowGuid);
    holder->SetPQuery(PETITION_SIGNED_AFTER,
                      "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u'",
                      petitionLowGuid, playerLowGuid);

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionGuid, ownerGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    WorldSession::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                               accountId, sessionId, playerGuid,
                                                                               petitionGuid, ownerGuid);
                                });
}

/**
 * @brief What the two snapshots around the conditional insert mean (D7b, C4).
 *
 * @param signedBefore Was this player on the petition before the insert ran?
 * @param signedAfter  Is this player on the petition now?
 * @return PETITION_SIGN_ALREADY_SIGNED when the signature was already there,
 *         PETITION_SIGN_OK when this request is what put it there,
 *         PETITION_SIGN_PETITION_FULL when the insert found no room.
 */
uint32 WorldSession::PetitionSignOutcome(bool signedBefore, bool signedAfter)
{
    if (signedBefore)
    {
        return PETITION_SIGN_ALREADY_SIGNED;
    }

    return signedAfter ? PETITION_SIGN_OK : PETITION_SIGN_PETITION_FULL;
}

/**
 * @brief Answers a sign request from the two snapshots around the insert.
 *
 * @param holder The staged statements.
 * @param accountId The signing account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The signer.
 * @param petitionGuid The petition guid.
 * @param ownerGuid The petition owner.
 */
void WorldSession::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                proto::SessionId sessionId, ObjectGuid playerGuid,
                                                ObjectGuid petitionGuid, ObjectGuid ownerGuid)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    std::unique_ptr<QueryResult> before(holder->GetResult(PETITION_SIGNED_BEFORE));
    std::unique_ptr<QueryResult> after(holder->GetResult(PETITION_SIGNED_AFTER));

    uint32 outcome = PetitionSignOutcome(before != NULL, after != NULL);

    if (outcome == PETITION_SIGN_PETITION_FULL)
    {
        // close at signer side
        player->SendPetitionSignResult(petitionGuid, player, PETITION_SIGN_PETITION_FULL);
        return;
    }

    if (outcome == PETITION_SIGN_OK)
    {
        DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), player->GetGuidStr().c_str());
    }

    // close at signer side
    player->SendPetitionSignResult(petitionGuid, player, outcome);

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if (item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // update for owner if online
    if (Player* owner = sPlayerRegistry.Find(ownerGuid))
    {
        owner->SendPetitionSignResult(petitionGuid, player, outcome);
    }
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

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid playerGuid = _player->GetObjectGuid();

    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid](QueryResult* result)
                                  {
                                      WorldSession::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult>(result),
                                                                                  accountId, sessionId, playerGuid);
                                  },
                                  "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
}

/**
 * @brief Tells the petition owner that the offer was declined.
 *
 * @param result The petition's owner row.
 * @param accountId The declining account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The declining player.
 */
void WorldSession::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                                 proto::SessionId sessionId, ObjectGuid playerGuid)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    if (!result)
    {
        return;
    }

    Field* fields = result->Fetch();
    ObjectGuid ownerguid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());

    if (Player* owner = sPlayerRegistry.Find(ownerguid))    // petition owner online
    {
        WorldPacket data(MSG_PETITION_DECLINE, 8);
        data << player->GetObjectGuid();
        owner->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Offers a petition to another player for signature.
 *
 * @param recv_data The incoming offer-petition packet.
 */
void WorldSession::HandleOfferPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_OFFER_PETITION");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    ObjectGuid playerGuid;
    uint32 junk;
    recv_data >> junk;                                      // this is not petition type!
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> playerGuid;                                // player guid

    Player* player = sPlayerRegistry.Find(playerGuid);
    if (!player)
    {
        return;
    }

    /// Get petition type and check, and the signs count: independent, so one holder.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_SHOW_SLOT_COUNT);
    holder->SetPQuery(PETITION_SHOW_PETITION,
                      "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    holder->SetPQuery(PETITION_SHOW_SIGNATURES,
                      "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid ownGuid = _player->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, ownGuid, petitionGuid, playerGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    WorldSession::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                              accountId, sessionId, ownGuid,
                                                                              petitionGuid, playerGuid);
                                });
}

/**
 * @brief Shows the offered petition to the other player.
 *
 * @param holder The staged reads.
 * @param accountId The offering account.
 * @param sessionId The session the request arrived on.
 * @param ownGuid The offering player.
 * @param petitionGuid The petition guid.
 * @param playerGuid The player the petition is offered to.
 */
void WorldSession::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                               proto::SessionId sessionId, ObjectGuid ownGuid,
                                               ObjectGuid petitionGuid, ObjectGuid playerGuid)
{
    WorldSession* session = NULL;
    Player* owner = NULL;
    if (!FindRequester(accountId, sessionId, ownGuid, session, owner))
    {
        return;
    }

    // The other player is re-found the same way, and the offer is dropped if they have gone.
    Player* player = sPlayerRegistry.Find(playerGuid);
    if (!player)
    {
        return;
    }

    std::unique_ptr<QueryResult> petition(holder->GetResult(PETITION_SHOW_PETITION));
    std::unique_ptr<QueryResult> signatures(holder->GetResult(PETITION_SHOW_SIGNATURES));

    if (!petition)
    {
        return;
    }

    DEBUG_LOG("OFFER PETITION: petition %s to %s", petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && owner->GetTeam() != player->GetTeam())
    {
        session->SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        session->SendGuildCommandResult(GUILD_INVITE_S, owner->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        session->SendGuildCommandResult(GUILD_INVITE_S, owner->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    /// Get petition signs count
    uint8 signs = 0;
    // result==NULL also correct charter without signs
    if (signatures)
    {
        signs = (uint8)signatures->GetRowCount();
    }

    /// Send response
    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + signs + signs * 12));
    data << petitionGuid;                                   // petition guid
    data << owner->GetObjectGuid();                         // owner guid
    data << uint32(petitionGuid.GetCounter());              // guild guid (in mangos always same as low part of petition guid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = signatures->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << signerGuid;                                 // Player GUID
        data << uint32(0);                                  // there 0 ...

        signatures->NextRow();
    }

    player->GetSession()->SendPacket(&data);
}

/**
 * @brief Handles turning in a completed petition to create a guild.
 *
 * @param recv_data The incoming turn-in-petition packet.
 */
void WorldSession::HandleTurnInPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION"); // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;

    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s turned in by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    /// Collect petition info data: the petition row and its signatures, in one holder.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_TURN_IN_COUNT);
    holder->SetPQuery(PETITION_TURN_IN_PETITION,
                      "SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    holder->SetPQuery(PETITION_TURN_IN_SIGNATURES,
                      "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());

    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    ObjectGuid playerGuid = _player->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    WorldSession::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                               accountId, sessionId, playerGuid,
                                                                               petitionGuid);
                                });
}

/**
 * @brief Creates the guild once the petition and its signatures are known.
 *
 * The guild creation chain itself (Guild::Create and Guild::AddMember, and what they reach)
 * is unchanged and still synchronous: it runs here, inside the continuation, and the tick
 * guard counts it. Converting that chain is D7f's job, not this PR's -- turn-in's OWN reads
 * are what move here.
 *
 * @param holder The staged reads.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The petition owner turning it in.
 * @param petitionGuid The petition guid.
 */
void WorldSession::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                proto::SessionId sessionId, ObjectGuid playerGuid,
                                                ObjectGuid petitionGuid)
{
    WorldSession* session = NULL;
    Player* player = NULL;
    if (!FindRequester(accountId, sessionId, playerGuid, session, player))
    {
        return;
    }

    std::unique_ptr<QueryResult> petition(holder->GetResult(PETITION_TURN_IN_PETITION));
    std::unique_ptr<QueryResult> signatures(holder->GetResult(PETITION_TURN_IN_SIGNATURES));

    /// Collect petition info data
    ObjectGuid ownerGuid;
    std::string name;

    // data
    if (petition)
    {
        Field* fields = petition->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
    }
    else
    {
        sLog.outError("CMSG_TURN_IN_PETITION: petition table not have data for guid %u!", petitionGuid.GetCounter());
        return;
    }

    if (player->GetGuildId())
    {
        player->SendPetitionTurnInResult(PETITION_TURN_ALREADY_IN_GUILD);  // already in guild
        return;
    }

    if (player->GetObjectGuid() != ownerGuid)
    {
        return;
    }

    // signs
    uint8 signs = signatures ? (uint8)signatures->GetRowCount() : 0;

    uint32 count = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);
    if (signs < count)
    {
        player->SendPetitionTurnInResult(PETITION_TURN_NEED_MORE_SIGNATURES);  // need more signatures...
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        player->SendPetitionTurnInResult(PETITION_TURN_GUILD_NAME_INVALID);
        return;
    }

    // and at last charter item check
    Item* item = player->GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    // OK!

    // delete charter item
    player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(player, name))
    {
        delete guild;
        return;
    }

    // register guild and add guildmaster
    sGuildMgr.AddGuild(guild);

    // add members
    for (uint8 i = 0; i < signs; ++i)
    {
        Field* fields = signatures->Fetch();

        ObjectGuid signGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        if (!signGuid)
        {
            continue;
        }

        guild->AddMember(signGuid, guild->GetLowestRank());
        signatures->NextRow();
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.CommitTransaction();

    // created
    DEBUG_LOG("TURN IN PETITION %s", petitionGuid.GetString().c_str());

    player->SendPetitionTurnInResult(PETITION_TURN_OK);
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
