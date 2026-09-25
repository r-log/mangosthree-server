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

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "CinematicFlyover.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Group.h"
#include "PlayerDump.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "SpellMgr.h"
#include "Calendar.h"
#include "CharacterCache.h"
#include "GameTime.h"
#include "Timer.h"

/**
 * @file CharacterHandlerCustomize.cpp
 * @brief Cohesion split of CharacterHandler.cpp -- character customization and client-settings opcode handlers: rename, declined names, alter appearance, remove glyph, customize, equipment sets, reorder characters, currency flags and load screen. Same WorldSession class; no behaviour change. CMake file(GLOB) picks this file up automatically; WorldSession.h is unchanged.
 */

namespace
{
    /// The slots of the customize handler's holder (decoupling D7d). Both statements are
    /// keyed on the character guid alone, so neither needs the other's answer: one holder,
    /// one round trip, where the handler used to block twice -- once for `at_login` and
    /// once inside Player::Customize for `playerBytes2`.
    enum CharCustomizeSlot
    {
        CHAR_CUSTOMIZE_AT_LOGIN      = 0,
        CHAR_CUSTOMIZE_PLAYER_BYTES2 = 1,
        CHAR_CUSTOMIZE_COUNT         = 2
    };

    /// The one-byte SMSG_CHAR_CUSTOMIZE failure answer.
    void SendCharCustomizeResult(WorldSession* session, uint8 result)
    {
        WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1);
        data << uint8(result);
        session->SendPacket(&data);
    }
}

/**
 * @brief Validates and starts the asynchronous character rename flow.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharRenameOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    std::string newname;

    recv_data >> guid;
    recv_data >> newname;

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_NO_NAME);
        SendPacket(&data);
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(res);
        SendPacket(&data);
        return;
    }

    // check name limitations
    if (GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_RESERVED);
        SendPacket(&data);
        return;
    }

    // Decoupling D7d (C5): the `NOT EXISTS (... WHERE name = '%s')` subquery is the only
    // reason this handler escaped a string on the world thread. Since D7c the same question
    // is answered from the character cache, whose name index folds exactly the way the
    // `characters`.`name` collation (utf8_general_ci) compares -- so the check moves up here,
    // into memory, and the statement below carries numbers only. A taken name answered
    // CHAR_CREATE_ERROR before (the subquery made the SELECT return nothing, and a NULL result
    // is what the callback maps to that code); it answers the same thing, a tick earlier.
    if (sObjectMgr.GetPlayerGuidByName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        SendPacket(&data);
        return;
    }

    // make sure that the character belongs to the current account and that rename at login
    // is enabled
    uint32 accountId = GetAccountId();
    proto::SessionId sessionId = GetSessionId();
    CharacterDatabase.AsyncPQuery([accountId, sessionId, newname](QueryResult* result)
                                  {
                                      WorldSession::HandleChangePlayerNameOpcodeCallBack(result, accountId, sessionId, newname);
                                  },
                                  "SELECT `guid`, `name` FROM `characters` WHERE `guid` = %u AND `account` = %u AND (`at_login` & %u) = %u",
                                  guid.GetCounter(), GetAccountId(), AT_LOGIN_RENAME, AT_LOGIN_RENAME
                                 );
}

/**
 * @brief Finalizes a character rename after the database validation query completes.
 *
 * Decoupling D7d: the account id alone was not an identity -- a reconnect in the tick between
 * the request and this answer is a NEW session on the same account, and it would have been
 * handed a rename it never asked for. FindRequesterSession() compares the session id too (C1).
 *
 * @param result The rename validation query result.
 * @param accountId The session account id.
 * @param sessionId The session the request arrived on.
 * @param newname The requested new character name.
 */
void WorldSession::HandleChangePlayerNameOpcodeCallBack(QueryResult* result, uint32 accountId,
                                                        proto::SessionId sessionId, std::string newname)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session)
    {
        if (result)
        {
            delete result;
        }
        return;
    }

    if (!result)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    uint32 guidLow = result->Fetch()[0].GetUInt32();
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    std::string oldname = result->Fetch()[1].GetCppString();

    delete result;

    // C3, and it is not optional. The handler checked the name against the cache in the tick
    // the request arrived in; this runs a tick later, and in between the name can have been
    // taken -- by a second rename to the same name on another session (whose own handler check
    // also passed, because neither had written yet), or by a create whose continuation added it
    // to the cache. The statement above no longer carries the `NOT EXISTS` subquery that used
    // to answer this on the delay thread AFTER the first UPDATE, so the question has to be
    // asked again here, immediately before the write. Same refusal the handler sends.
    if (sObjectMgr.GetPlayerGuidByName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    // C5: the name is bound, not pasted. Whatever the connection has to do with it -- a real
    // bind, or the escaping the plain fallback does -- happens inside the delay thread's own
    // operation, never here. (The old statement pasted the UNESCAPED name; only the SELECT
    // above was given the escaped copy.)
    static SqlStatementID renameCharacter;
    SqlStatement stmt = CharacterDatabase.CreateStatement(renameCharacter,
                        "UPDATE `characters` SET `name` = ?, `at_login` = `at_login` & ~ ? WHERE `guid` = ?");

    CharacterDatabase.BeginTransaction();
    stmt.PExecute(newname.c_str(), uint32(AT_LOGIN_RENAME), guidLow);
    CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` ='%u'", guidLow);
    CharacterDatabase.CommitTransaction();

    // Decoupling D7c: the name that answers GetPlayerGuidByName / GetPlayerNameByGUID is
    // the one just written, from this moment on.
    sCharacterCache.UpdateName(guid, newname);

    sLog.outChar("Account: %d (IP: %s) Character:[%s] (guid:%u) Changed name to: %s", session->GetAccountId(), session->GetRemoteAddress().c_str(), oldname.c_str(), guidLow, newname.c_str());

    WorldPacket data(SMSG_CHAR_RENAME, 1 + 8 + (newname.size() + 1));
    data << uint8(RESPONSE_SUCCESS);
    data << guid;
    data << newname;
    session->SendPacket(&data);

    sWorld.InvalidatePlayerDataToAllClient(guid);
}

void WorldSession::HandleSetPlayerDeclinedNamesOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    // not accept declined names for unsupported languages
    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(guid, name))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        SendPacket(&data);
        return;
    }

    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        SendPacket(&data);
        return;
    }

    if (!isCyrillicCharacter(wname[0]))                     // name already stored as only single alphabet using
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        SendPacket(&data);
        return;
    }

    std::string name2;
    DeclinedName declinedname;

    recv_data >> name2;

    if (name2 != name)                                      // character have different name
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        SendPacket(&data);
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        recv_data >> declinedname.name[i];
        if (!normalizePlayerName(declinedname.name[i]))
        {
            WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
            data << uint32(1);
            SendPacket(&data);
            return;
        }
    }

    if (!ObjectMgr::CheckDeclinedNames(GetMainPartOfName(wname, 0), declinedname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        SendPacket(&data);
        return;
    }

    // C5 (decoupling D7d): the five declined forms were escaped here, on the world thread --
    // five Database::escape_string() calls, each of which takes query connection zero's lock.
    // They are bound instead, and whatever escaping the connection needs happens inside the
    // delay thread's own operation.
    static SqlStatementID insertDeclinedName;
    SqlStatement stmt = CharacterDatabase.CreateStatement(insertDeclinedName,
                        "INSERT INTO `character_declinedname` (`guid`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) VALUES (?,?,?,?,?,?)");
    stmt.addUInt32(guid.GetCounter());
    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        stmt.addString(declinedname.name[i]);
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` = '%u'", guid.GetCounter());
    stmt.Execute();
    CharacterDatabase.CommitTransaction();

    WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
    data << uint32(0);                                      // OK
    SendPacket(&data);
}

void WorldSession::HandleAlterAppearanceOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ALTER_APPEARANCE");

    uint32 Hair, Color, FacialHair, skinTone;
    recv_data >> Hair >> Color >> FacialHair >> skinTone;

    uint32 skinTone_id = -1;
    if (_player->getRace() == RACE_TAUREN)
    {
        BarberShopStyleEntry const* bs_skinTone = sBarberShopStyleStore.LookupEntry(skinTone);
        if (!bs_skinTone || bs_skinTone->Type != 3 || bs_skinTone->Race != _player->getRace() || bs_skinTone->Sex != _player->getGender())
        {
            return;
        }
        skinTone_id = bs_skinTone->Data;
    }

    BarberShopStyleEntry const* bs_hair = sBarberShopStyleStore.LookupEntry(Hair);

    if (!bs_hair || bs_hair->Type != 0 || bs_hair->Race != _player->getRace() || bs_hair->Sex != _player->getGender())
    {
        return;
    }

    BarberShopStyleEntry const* bs_facialHair = sBarberShopStyleStore.LookupEntry(FacialHair);

    if (!bs_facialHair || bs_facialHair->Type != 2 || bs_facialHair->Race != _player->getRace() || bs_facialHair->Sex != _player->getGender())
    {
        return;
    }

    uint32 Cost = _player->GetBarberShopCost(bs_hair->Data, Color, bs_facialHair->Data, skinTone_id);

    // 0 - ok
    // 1,3 - not enough money
    // 2 - you have to seat on barber chair
    if (_player->GetMoney() < Cost)
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(1);                                  // no money
        SendPacket(&data);
        return;
    }
    else
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(0);                                  // ok
        SendPacket(&data);
    }

    _player->ModifyMoney(-int64(Cost));                     // it isn't free
    _player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER, Cost);

    _player->SetByteValue(PLAYER_BYTES, 2, uint8(bs_hair->Data));
    _player->SetByteValue(PLAYER_BYTES, 3, uint8(Color));
    _player->SetByteValue(PLAYER_BYTES_2, 0, uint8(bs_facialHair->Data));
    if (_player->getRace() == RACE_TAUREN)
    {
        _player->SetByteValue(PLAYER_BYTES, 0, uint8(skinTone_id));
    }

    _player->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP, 1);

    _player->SetStandState(0);                              // stand up
}

void WorldSession::HandleRemoveGlyphOpcode(WorldPacket& recv_data)
{
    uint32 slot;
    recv_data >> slot;

    if (slot >= MAX_GLYPH_SLOT_INDEX)
    {
        DEBUG_LOG("Client sent wrong glyph slot number in opcode CMSG_REMOVE_GLYPH %u", slot);
        return;
    }

    if (_player->GetGlyph(slot))
    {
        _player->ApplyGlyph(slot, false);
        _player->SetGlyph(slot, 0);
        _player->SendTalentsInfoData(false);
    }
}

/**
 * @brief Starts the asynchronous character customize flow (decoupling D7d).
 *
 * Nothing is checked or written here: both reads the old handler blocked on are staged into
 * one holder and every decision it made is made in the continuation, in the same order.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCharCustomizeOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;
    CharCustomizeRequest request;

    recv_data >> guid;
    recv_data >> request.newname;

    recv_data >> request.gender >> request.skin >> request.hairColor >> request.hairStyle >> request.facialHair >> request.face;

    QueueCharCustomizeReads(GetAccountId(), GetSessionId(), guid, request);
}

/**
 * @brief Stages the customize handler's two reads (decoupling D7d, C4).
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character being customized.
 * @param request   The requested name and appearance.
 */
void WorldSession::QueueCharCustomizeReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid,
                                           CharCustomizeRequest request)
{
    SqlQueryHolder* holder = new SqlQueryHolder;
    holder->SetSize(CHAR_CUSTOMIZE_COUNT);
    holder->SetPQuery(CHAR_CUSTOMIZE_AT_LOGIN,
                      "SELECT `at_login` FROM `characters` WHERE `guid` = '%u'", guid.GetCounter());
    //                                                       0
    holder->SetPQuery(CHAR_CUSTOMIZE_PLAYER_BYTES2,
                      "SELECT `playerBytes2` FROM `characters` WHERE `guid` = '%u'", guid.GetCounter());

    // DelayQueryHolder() answers false without queueing once the database is shutting down
    // (decoupling D7a), and the holder is the callback's to free -- so free it here instead.
    if (!CharacterDatabase.DelayQueryHolder([accountId, sessionId, guid, request]
                                           (QueryResult* /*result*/, SqlQueryHolder* h)
                                           {
                                               WorldSession::HandleCharCustomizeCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                         accountId, sessionId, guid, request);
                                           }, holder))
    {
        delete holder;                                      // delete all unprocessed queries
    }
}

/**
 * @brief Applies the customize, once the character's flags and appearance are known.
 *
 * Every check is the handler's, in the handler's order, with the handler's reply: the
 * at_login flag, then the name's validity, then the reserved list, then whether the name is
 * taken. The cache update still sits immediately in front of the write it describes, and the
 * write no longer escapes the name on this thread (C5).
 *
 * @param holder    The two staged reads, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character being customized.
 * @param request   The requested name and appearance.
 */
void WorldSession::HandleCharCustomizeCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                               proto::SessionId sessionId, ObjectGuid guid,
                                               CharCustomizeRequest request)
{
    WorldSession* session = FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    std::string newname = request.newname;

    std::unique_ptr<QueryResult> result(holder->GetResult(CHAR_CUSTOMIZE_AT_LOGIN));
    if (!result)
    {
        SendCharCustomizeResult(session, CHAR_CREATE_ERROR);
        return;
    }

    Field* fields = result->Fetch();
    uint32 at_loginFlags = fields[0].GetUInt32();
    result.reset();

    if (!(at_loginFlags & AT_LOGIN_CUSTOMIZE))
    {
        SendCharCustomizeResult(session, CHAR_CREATE_ERROR);
        return;
    }

    // prevent character rename to invalid name
    if (!normalizePlayerName(newname))
    {
        SendCharCustomizeResult(session, CHAR_NAME_NO_NAME);
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        SendCharCustomizeResult(session, res);
        return;
    }

    // check name limitations
    if (session->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        SendCharCustomizeResult(session, CHAR_NAME_RESERVED);
        return;
    }

    // character with this name already exist
    ObjectGuid newguid = sObjectMgr.GetPlayerGuidByName(newname);
    if (newguid && newguid != guid)
    {
        SendCharCustomizeResult(session, CHAR_CREATE_NAME_IN_USE);
        return;
    }

    // Decoupling D7c: immediately before the write it describes, and the name it indexes is
    // the one the player typed -- which is now also the one the statement binds.
    sCharacterCache.UpdateName(guid, newname);

    // Player::Customize used to read `playerBytes2` itself; the value comes out of this
    // holder now. No row means no such character, which is what made the old call return
    // without writing -- the same thing skipping it does.
    std::unique_ptr<QueryResult> resultBytes(holder->GetResult(CHAR_CUSTOMIZE_PLAYER_BYTES2));
    if (resultBytes)
    {
        Player::Customize(guid, request.gender, request.skin, request.face, request.hairStyle,
                          request.hairColor, request.facialHair, resultBytes->Fetch()[0].GetUInt32());
    }

    // C5: the name is bound, not escaped here.
    static SqlStatementID renameCustomized;
    SqlStatement stmt = CharacterDatabase.CreateStatement(renameCustomized,
                        "UPDATE `characters` SET `name` = ?, `at_login` = `at_login` & ~ ? WHERE `guid` = ?");
    stmt.PExecute(newname.c_str(), uint32(AT_LOGIN_CUSTOMIZE), guid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM `character_declinedname` WHERE `guid` ='%u'", guid.GetCounter());

    std::string IP_str = session->GetRemoteAddress();
    sLog.outChar("Account: %d (IP: %s), Character %s customized to: %s", accountId, IP_str.c_str(), guid.GetString().c_str(), newname.c_str());

    WorldPacket data(SMSG_CHAR_CUSTOMIZE, 1 + 8 + (newname.size() + 1) + 6);
    data << uint8(RESPONSE_SUCCESS);
    data << ObjectGuid(guid);
    data << newname;
    data << uint8(request.gender);
    data << uint8(request.skin);
    data << uint8(request.face);
    data << uint8(request.hairStyle);
    data << uint8(request.hairColor);
    data << uint8(request.facialHair);
    session->SendPacket(&data);

    sWorld.InvalidatePlayerDataToAllClient(guid);
}

void WorldSession::HandleEquipmentSetSaveOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_SAVE");

    ObjectGuid setGuid;
    uint32 index;
    std::string name;
    std::string iconName;

    recv_data >> setGuid.ReadAsPacked();
    recv_data >> index;
    recv_data >> name;
    recv_data >> iconName;

    if (index >= MAX_EQUIPMENT_SET_INDEX)                   // client set slots amount
    {
        return;
    }

    EquipmentSet eqSet;

    eqSet.Guid      = setGuid.GetRawValue();
    eqSet.Name      = name;
    eqSet.IconName  = iconName;
    eqSet.state     = EQUIPMENT_SET_NEW;

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        ObjectGuid itemGuid;

        recv_data >> itemGuid.ReadAsPacked();

        // equipment manager sends "1" (as raw GUID) for slots set to "ignore" (not touch slot at equip set)
        if (itemGuid.GetRawValue() == 1)
        {
            // ignored slots saved as bit mask because we have no free special values for Items[i]
            eqSet.IgnoreMask |= 1 << i;
            continue;
        }

        Item* item = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);

        if (!item && itemGuid)                              // cheating check 1
        {
            return;
        }

        if (item && item->GetObjectGuid() != itemGuid)      // cheating check 2
        {
            return;
        }

        eqSet.Items[i] = itemGuid.GetCounter();
    }

    _player->SetEquipmentSet(index, eqSet);
}

void WorldSession::HandleEquipmentSetDeleteOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_DELETE");

    ObjectGuid setGuid;

    recv_data >> setGuid.ReadAsPacked();

    _player->DeleteEquipmentSet(setGuid.GetRawValue());
}

void WorldSession::HandleEquipmentSetUseOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_USE");
    recv_data.hexlike();

    for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
    {
        ObjectGuid itemGuid;
        uint8 srcbag, srcslot;

        recv_data >> itemGuid.ReadAsPacked();
        recv_data >> srcbag >> srcslot;

        DEBUG_LOG("Item (%s): srcbag %u, srcslot %u", itemGuid.GetString().c_str(), srcbag, srcslot);

        // check if item slot is set to "ignored" (raw value == 1), must not be unequipped then
        if (itemGuid.GetRawValue() == 1)
        {
            continue;
        }

        Item* item = _player->GetItemByGuid(itemGuid);

        uint16 dstpos = i | (INVENTORY_SLOT_BAG_0 << 8);

        if (!item)
        {
            Item* uItem = _player->GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!uItem)
            {
                continue;
            }

            ItemPosCountVec sDest;
            InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, sDest, uItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                _player->RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                _player->StoreItem(sDest, uItem, true);
            }
            else
            {
                _player->SendEquipError(msg, uItem, NULL);
            }

            continue;
        }

        if (item->GetPos() == dstpos)
        {
            continue;
        }

        _player->SwapItem(item->GetPos(), dstpos);
    }

    WorldPacket data(SMSG_USE_EQUIPMENT_SET_RESULT, 1);
    data << uint8(0);                                       // 4 - equipment swap failed - inventory is full
    SendPacket(&data);
}

void WorldSession::HandleReorderCharactersOpcode(WorldPacket& recv_data)
{
    uint32 charCount = recv_data.ReadBits(10);

    if (charCount > sWorld.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
    {
        DEBUG_LOG("SESSION: received CMSG_REORDER_CHARACTERS, but characters count %u is beyond server limit.", charCount);
        recv_data.rfinish();
        return;
    }

    std::vector<ObjectGuid> guids;
    std::vector<uint8> slots;

    for (uint32 i = 0; i < charCount; ++i)
    {
        ObjectGuid guid;
        recv_data.ReadGuidMask<1, 4, 5, 3, 0, 7, 6, 2>(guid);
        guids.push_back(guid);
    }

    for (uint32 i = 0; i < charCount; ++i)
    {
        recv_data.ReadGuidBytes<6, 5, 1, 4, 0, 3>(guids[i]);
        slots.push_back(recv_data.ReadUInt8());
        recv_data.ReadGuidBytes<2, 7>(guids[i]);
    }

    CharacterDatabase.BeginTransaction();
    for (uint32 i = 0; i < charCount; ++i)
        CharacterDatabase.PExecute("UPDATE `characters` SET `slot` = '%u' WHERE `guid` = '%u' AND `account` = '%u'",
        slots[i], guids[i].GetCounter(), GetAccountId());
    CharacterDatabase.CommitTransaction();
}

void WorldSession::HandleSetCurrencyFlagsOpcode(WorldPacket& recv_data)
{
    uint32 currencyId, flags;
    recv_data >> flags >> currencyId;

    DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: currency: %u, flags: %u", currencyId, flags);

    if (flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT)
    {
        DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: received unknown currency flags 0x%X from player %s account %u for currency %u",
            flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT, GetPlayer()->GetGuidStr().c_str(), GetAccountId(), currencyId);
    }

    flags &= PLAYERCURRENCY_MASK_USED_BY_CLIENT;
    GetPlayer()->SetCurrencyFlags(currencyId, uint8(flags));
}

void WorldSession::HandleLoadScreenOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_LOAD_SCREEN");
    uint32 mapID;

    recvPacket >> mapID;
    recvPacket.ReadBit();
}
