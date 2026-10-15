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

#include "session/handlers/social/PetitionHandlers.h"

#include "Platform/Define.h"
#include <functional>
#include <memory>
#include <string>
#include <sstream>
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "WorldHandlers/World.h"
#include "Object/ObjectMgr.h"
#include "Log/Log.h"
#include "Opcodes.h"
#include "Object/Guild.h"
#include "WorldHandlers/GuildMgr.h"
#include "entities/player/PlayerRegistry.h"

// Charters ID in item_template
constexpr uint32 GUILD_CHARTER = 5863;

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

    /// The slots of the sign handler's SECOND holder -- the atomic sign itself.
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
     * @brief Queue a staged holder, freeing it if the database refuses it.
     *
     * DelayQueryHolder() answers false without queueing once the database is shutting down;
     * the callback that frees the holder then never runs, so the holder is deleted here.
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
void PetitionHandlers::HandlePetitionBuy(WorldSession& session, WorldPacket& recv_data)
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
    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuy - %s not found or you can't interact with him.", guidNPC.GetString().c_str());
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (!pCreature->IsTabardDesigner())
    {
        sLog.outError("WORLD: HandlePetitionBuy - unsupported npc type, npc: %s", guidNPC.GetString().c_str());
        return;
    }

    // Every check below reads memory only, so it stays here: a request that cannot succeed
    // never reaches the database at all. Each of them runs again in the continuation,
    // because a tick passes before the charter is paid for.
    if (sGuildMgr.GetGuildByName(name))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        session.SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(GUILD_CHARTER);
    if (!pProto)
    {
        session.GetPlayer()->SendBuyError(BUY_ERR_CANT_FIND_ITEM, NULL, GUILD_CHARTER, 0);
        return;
    }

    if (session.GetPlayer()->GetMoney() < sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST))
    {
        // player hasn't got enough money
        session.GetPlayer()->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, GUILD_CHARTER, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = session.GetPlayer()->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, GUILD_CHARTER, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        session.GetPlayer()->SendEquipError(msg, NULL, NULL, GUILD_CHARTER);
        return;
    }

    // The read is queued before anything is charged or created, so a buyer who logs out
    // before it answers pays nothing.
    QueuePetitionBuyRead(session.GetAccountId(), session.GetSessionId(), session.GetPlayer()->GetObjectGuid(), guidNPC, name);
}

/**
 * @brief Stages the buy handler's only read.
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
void PetitionHandlers::QueuePetitionBuyRead(uint32 accountId, proto::SessionId sessionId,
                                            ObjectGuid playerGuid, ObjectGuid npcGuid, std::string name)
{
    // the buyer's petitions are read here; the continuation deletes them and their signatures
    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid, npcGuid, name](QueryResult* result)
                                  {
                                      PetitionHandlers::HandlePetitionBuyCallback(std::unique_ptr<QueryResult>(result),
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
void PetitionHandlers::HandlePetitionBuyCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                                 proto::SessionId sessionId, ObjectGuid playerGuid,
                                                 ObjectGuid npcGuid, std::string name)
{
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player))
    {
        return;
    }

    // Everything the handler checked before it queued, checked again: a tick has passed, so
    // the NPC may be out of range, the name may have been taken, the bag may have filled and
    // the money may have been spent. Only then is anything charged or created.
    Creature* pCreature = player->GetNPCIfCanInteractWith(npcGuid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuy - %s not found or you can't interact with him.", npcGuid.GetString().c_str());
        return;
    }

    if (!pCreature->IsTabardDesigner())
    {
        sLog.outError("WORLD: HandlePetitionBuy - unsupported npc type, npc: %s", npcGuid.GetString().c_str());
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
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

    // The name is never escaped on the world thread. It is bound as a parameter, and
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
void PetitionHandlers::HandlePetitionShowSign(WorldSession& session, WorldPacket& recv_data)
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

    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionguid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    PetitionHandlers::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder>(h),
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
void PetitionHandlers::HandlePetitionShowSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                      proto::SessionId sessionId, ObjectGuid playerGuid,
                                                      ObjectGuid petitionGuid)
{
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player))
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

    requestSession->SendPacket(&data);
}

/**
 * @brief Handles a petition query request.
 *
 * @param recv_data The incoming petition query packet.
 */
void PetitionHandlers::HandlePetitionQuery(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");
    // recv_data.hexlike();

    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >> guildguid;                                 // in mangos always same as GUID_LOPART(petitionguid)
    recv_data >> petitionguid;                              // petition guid
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", petitionguid.GetString().c_str(), guildguid);

    session.SendPetitionQueryOpcode(petitionguid);
}

/**
 * @brief Handles petition renaming and updates persistent storage.
 *
 * @param recv_data The incoming petition rename packet.
 */
void PetitionHandlers::HandlePetitionRename(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_RENAME");   // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    std::string newname;

    recv_data >> petitionGuid;                              // guid
    recv_data >> newname;                                   // new name

    Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    // Read first, then write in the continuation, the name bound as a parameter.
    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();

    CharacterDatabase.AsyncPQuery([accountId, sessionId, playerGuid, petitionGuid, newname](QueryResult* result)
                                  {
                                      PetitionHandlers::HandlePetitionRenameCallback(std::unique_ptr<QueryResult>(result),
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
void PetitionHandlers::HandlePetitionRenameCallback(std::unique_ptr<QueryResult> result, uint32 accountId,
                                                    proto::SessionId sessionId, ObjectGuid playerGuid,
                                                    ObjectGuid petitionGuid, std::string newname)
{
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player))
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
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(newname) || !ObjectMgr::IsValidCharterName(newname))
    {
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, newname, ERR_GUILD_NAME_INVALID);
        return;
    }

    // Bound, not escaped here -- see HandlePetitionBuyCallback.
    static SqlStatementID renamePetition;
    SqlStatement stmt = CharacterDatabase.CreateStatement(renamePetition,
                        "UPDATE `petition` SET `name` = ? WHERE `petitionguid` = ?");
    stmt.PExecute(newname.c_str(), petitionGuid.GetCounter());

    DEBUG_LOG("Petition %s renamed to '%s'", petitionGuid.GetString().c_str(), newname.c_str());

    WorldPacket data(MSG_PETITION_RENAME, 8 + newname.size() + 1);
    data << petitionGuid;
    data << newname;
    requestSession->SendPacket(&data);
}

/**
 * @brief Handles signing a guild petition.
 *
 * @param recv_data The incoming petition sign packet.
 */
void PetitionHandlers::HandlePetitionSign(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");    // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> petitionGuid;                              // petition guid
    recv_data >> unk;

    // The one check that needs nothing but memory stays on the tick thread.
    if (session.GetPlayer()->GetGuildId() || session.GetPlayer()->GetGuildIdInvited())
    {
        // close at signer side
        session.GetPlayer()->SendPetitionSignResult(petitionGuid, session.GetPlayer(), PETITION_SIGN_ALREADY_IN_GUILD);
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
                      "SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '%u'", session.GetAccountId());

    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    PetitionHandlers::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                             accountId, sessionId, playerGuid,
                                                                             petitionGuid);
                                });
}

/**
 * @brief Decides whether a sign request may go ahead, once the petition is known.
 *
 * Every check that needs the petition row or the account's signatures runs here. Only when
 * none of them stops the request does it reach the atomic sign itself.
 *
 * @param holder The staged reads.
 * @param accountId The signing account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The signer.
 * @param petitionGuid The petition guid.
 */
void PetitionHandlers::HandlePetitionSignCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                  proto::SessionId sessionId, ObjectGuid playerGuid,
                                                  ObjectGuid petitionGuid)
{
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player))
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
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
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
 * @brief Stages the atomic sign: one holder, three statements.
 *
 * The three statements run back to back on the delay thread under the connection's lock. The
 * insert adds the signature only if the petition exists, is under the limit and does not hold
 * this player yet; the rows read before and after it name the outcome.
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
void PetitionHandlers::QueuePetitionSignHolder(uint32 accountId, proto::SessionId sessionId, ObjectGuid playerGuid,
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
                                    PetitionHandlers::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                               accountId, sessionId, playerGuid,
                                                                               petitionGuid, ownerGuid);
                                });
}

/**
 * @brief What the two snapshots around the conditional insert mean.
 *
 * @param signedBefore Was this player on the petition before the insert ran?
 * @param signedAfter  Is this player on the petition now?
 * @return PETITION_SIGN_ALREADY_SIGNED when the signature was already there,
 *         PETITION_SIGN_OK when this request is what put it there,
 *         PETITION_SIGN_PETITION_FULL when the insert found no room.
 */
uint32 PetitionHandlers::PetitionSignOutcome(bool signedBefore, bool signedAfter)
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
void PetitionHandlers::HandlePetitionSignedCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                    proto::SessionId sessionId, ObjectGuid playerGuid,
                                                    ObjectGuid petitionGuid, ObjectGuid ownerGuid)
{
    // The insert has ALREADY run, back to back with the two snapshots, by the time this
    // continuation is reached: the signature is in the table whether or not the signer is
    // still here. His own copy of the reply needs him; the OWNER's copy does not, and
    // dropping it with him would leave the charter's owner looking at a signature list that
    // is one short until he re-opens it. So the re-find is not a gate -- it
    // decides only whether the signer is told, and every packet below is built from the
    // captured guid, which SMSG_PETITION_SIGN_RESULTS is all that carries.
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    bool signerHere = WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player);

    std::unique_ptr<QueryResult> before(holder->GetResult(PETITION_SIGNED_BEFORE));
    std::unique_ptr<QueryResult> after(holder->GetResult(PETITION_SIGNED_AFTER));

    uint32 outcome = PetitionSignOutcome(before != NULL, after != NULL);

    if (outcome == PETITION_SIGN_PETITION_FULL)
    {
        // close at signer side (no owner copy for a full petition)
        if (signerHere)
        {
            player->SendPetitionSignResult(petitionGuid, playerGuid, PETITION_SIGN_PETITION_FULL);
        }
        return;
    }

    if (outcome == PETITION_SIGN_OK)
    {
        DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());
    }

    // close at signer side
    if (signerHere)
    {
        player->SendPetitionSignResult(petitionGuid, playerGuid, outcome);
    }

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if (item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // update for owner if online
    if (Player* owner = sPlayerRegistry.Find(ownerGuid))
    {
        owner->SendPetitionSignResult(petitionGuid, playerGuid, outcome);
    }
}

/**
 * @brief Handles declining a petition offer and notifies the owner.
 *
 * @param recv_data The incoming petition decline packet.
 */
void PetitionHandlers::HandlePetitionDecline(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode MSG_PETITION_DECLINE");  // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;
    recv_data >> petitionGuid;                              // petition guid

    DEBUG_LOG("Petition %s declined by %s", petitionGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());

    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();

    CharacterDatabase.AsyncPQuery([playerGuid](QueryResult* result)
                                  {
                                      PetitionHandlers::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult>(result),
                                                                                  playerGuid);
                                  },
                                  "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
}

/**
 * @brief Tells the petition owner that the offer was declined.
 *
 * The one packet this continuation sends goes to the OWNER, not to the requester, and it
 * names the decliner by GUID -- so it is built from the captured guid and the requester is
 * neither re-found nor needed. Dropping it because the decliner logged out in the
 * intervening tick would leave the owner's charter UI waiting for an answer that has
 * already been given.
 *
 * @param result The petition's owner row.
 * @param playerGuid The declining player.
 */
void PetitionHandlers::HandlePetitionDeclineCallback(std::unique_ptr<QueryResult> result, ObjectGuid playerGuid)
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
 * @brief Offers a petition to another player for signature.
 *
 * @param recv_data The incoming offer-petition packet.
 */
void PetitionHandlers::HandleOfferPetition(WorldSession& session, WorldPacket& recv_data)
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

    /// The petition's existence and its signatures: independent, so one holder.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_SHOW_SLOT_COUNT);
    holder->SetPQuery(PETITION_SHOW_PETITION,
                      "SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    holder->SetPQuery(PETITION_SHOW_SIGNATURES,
                      "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());

    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    ObjectGuid ownGuid = session.GetPlayer()->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, ownGuid, petitionGuid, playerGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    PetitionHandlers::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder>(h),
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
void PetitionHandlers::HandleOfferPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                   proto::SessionId sessionId, ObjectGuid ownGuid,
                                                   ObjectGuid petitionGuid, ObjectGuid playerGuid)
{
    WorldSession* requestSession = NULL;
    Player* owner = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, ownGuid, requestSession, owner))
    {
        return;
    }

    // The other player is re-found by guid, and the offer is dropped if they have gone.
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
        requestSession->SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        requestSession->SendGuildCommandResult(GUILD_INVITE_S, owner->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        requestSession->SendGuildCommandResult(GUILD_INVITE_S, owner->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
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
void PetitionHandlers::HandleTurnInPetition(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION"); // ok
    // recv_data.hexlike();

    ObjectGuid petitionGuid;

    recv_data >> petitionGuid;

    DEBUG_LOG("Petition %s turned in by %s", petitionGuid.GetString().c_str(), session.GetPlayer()->GetGuidStr().c_str());

    /// Collect petition info data: the petition row and its signatures, in one holder.
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(PETITION_TURN_IN_COUNT);
    holder->SetPQuery(PETITION_TURN_IN_PETITION,
                      "SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    holder->SetPQuery(PETITION_TURN_IN_SIGNATURES,
                      "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());

    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    ObjectGuid playerGuid = session.GetPlayer()->GetObjectGuid();

    QueuePetitionHolder(holder, [accountId, sessionId, playerGuid, petitionGuid](QueryResult* /*result*/, SqlQueryHolder* h)
                                {
                                    PetitionHandlers::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                               accountId, sessionId, playerGuid,
                                                                               petitionGuid);
                                });
}

/**
 * @brief Creates the guild once the petition and its signatures are known.
 *
 * The guild creation chain (Guild::Create and Guild::AddMember, and what they reach) runs
 * here, inside the continuation, and it acquires nothing: the guild's own INSERTs
 * and the five default ranks are prepared statements, an offline signer's row comes from
 * the character cache, and each new member's petition signatures are read by a continuation
 * of their own (Player::QueueRemovePetitionsAndSigns).
 *
 * @param holder The staged reads.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param playerGuid The petition owner turning it in.
 * @param petitionGuid The petition guid.
 */
void PetitionHandlers::HandleTurnInPetitionCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                                    proto::SessionId sessionId, ObjectGuid playerGuid,
                                                    ObjectGuid petitionGuid)
{
    WorldSession* requestSession = NULL;
    Player* player = NULL;
    if (!WorldSession::FindRequesterPlayer(accountId, sessionId, playerGuid, requestSession, player))
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
    Item* item = player->GetInventoryMgr().GetItemByGuid(petitionGuid);
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
void PetitionHandlers::HandlePetitionShowList(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");
    // recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    session.SendPetitionShowList(guid);
}
