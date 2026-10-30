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

#include "session/handlers/entities/CharacterCustomizeHandlers.h"

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
#include "session/SessionRegistry.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "DBCStores.h"
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
#include "spells/data/SpellMgr.h"
#include "Calendar.h"
#include "CharacterCache.h"
#include "GameTime.h"
#include "Timer.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/CharacterTransaction.h"

static_assert(CHARACTER_DECLINED_NAME_ROW_CASES == MAX_DECLINED_NAME_CASES, "a character's saved declined names are its declined names");

namespace
{
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
void CharacterCustomizeHandlers::HandleCharRename(WorldSession& session, WorldPacket& recv_data)
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
        session.SendPacket(&data);
        return;
    }

    uint8 res = ObjectMgr::CheckPlayerName(newname, true);
    if (res != CHAR_NAME_SUCCESS)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(res);
        session.SendPacket(&data);
        return;
    }

    // check name limitations
    if (session.GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_NAME_RESERVED);
        session.SendPacket(&data);
        return;
    }

    // The character cache answers whether the name is taken, and its name index folds the way
    // the `characters`.`name` collation (utf8_general_ci) compares, so the check is made here,
    // in memory, and the statement below carries numbers only. A taken name answers
    // CHAR_CREATE_ERROR, the code the callback gives a missing row.
    if (sObjectMgr.GetPlayerGuidByName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session.SendPacket(&data);
        return;
    }

    // make sure that the character belongs to the current account and that rename at login
    // is enabled
    uint32 accountId = session.GetAccountId();
    proto::SessionId sessionId = session.GetSessionId();
    CharacterRepository::QueryRenameTarget(guid.GetCounter(), accountId, AT_LOGIN_RENAME,
                                           [accountId, sessionId, newname](std::optional<RenameTargetRow> row)
                                           {
                                               CharacterCustomizeHandlers::HandleChangePlayerNameCallback(row, accountId, sessionId, newname);
                                           });
}

/**
 * @brief Finalizes a character rename after the database validation query completes.
 *
 * The account id alone is not an identity: a reconnect in the tick between the request and
 * this answer is a new session on the same account, which never asked for the rename.
 * FindRequesterSession() compares the session id too.
 *
 * @param row The character the rename may rename, none when it may not.
 * @param accountId The session account id.
 * @param sessionId The session the request arrived on.
 * @param newname The requested new character name.
 */
void CharacterCustomizeHandlers::HandleChangePlayerNameCallback(std::optional<RenameTargetRow> const& row, uint32 accountId,
                                                        proto::SessionId sessionId, std::string newname)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session)
    {
        return;
    }

    if (!row)
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    uint32 guidLow = row->guid;
    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidLow);
    std::string oldname = row->name;

    // The handler checked the name against the cache in the tick the request arrived in; this
    // runs a tick later, and in between the name can have been taken, by a second rename to the
    // same name on another session (whose own handler check also passed, because neither had
    // written yet), or by a create whose continuation added it to the cache. The statement above
    // does not check the name, so it is checked again here, immediately before the write, with
    // the refusal the handler sends.
    if (sObjectMgr.GetPlayerGuidByName(newname))
    {
        WorldPacket data(SMSG_CHAR_RENAME, 1);
        data << uint8(CHAR_CREATE_ERROR);
        session->SendPacket(&data);
        return;
    }

    // The name is bound, not pasted: whatever the connection has to do with it -- a real bind,
    // or the escaping the plain fallback does -- happens inside the delay thread's own
    // operation, never here.
    CharacterTransaction::Begin();
    CharacterRepository::Rename(guidLow, newname, uint32(AT_LOGIN_RENAME));
    CharacterTransaction::Commit();

    // The name that answers GetPlayerGuidByName / GetPlayerNameByGUID is the one just
    // written, from this moment on.
    sCharacterCache.UpdateName(guid, newname);

    sLog.outChar("Account: %d (IP: %s) Character:[%s] (guid:%u) Changed name to: %s", session->GetAccountId(), session->GetRemoteAddress().c_str(), oldname.c_str(), guidLow, newname.c_str());

    WorldPacket data(SMSG_CHAR_RENAME, 1 + 8 + (newname.size() + 1));
    data << uint8(RESPONSE_SUCCESS);
    data << guid;
    data << newname;
    session->SendPacket(&data);

    sSessionRegistry.InvalidatePlayerData(guid);
}

void CharacterCustomizeHandlers::HandleSetPlayerDeclinedNames(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    // not accept declined names for unsupported languages
    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(guid, name))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        session.SendPacket(&data);
        return;
    }

    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        session.SendPacket(&data);
        return;
    }

    if (!isCyrillicCharacter(wname[0]))                     // name already stored as only single alphabet using
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        session.SendPacket(&data);
        return;
    }

    std::string name2;
    DeclinedName declinedname;

    recv_data >> name2;

    if (name2 != name)                                      // character have different name
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        session.SendPacket(&data);
        return;
    }

    for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
    {
        recv_data >> declinedname.name[i];
        if (!normalizePlayerName(declinedname.name[i]))
        {
            WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
            data << uint32(1);
            session.SendPacket(&data);
            return;
        }
    }

    if (!ObjectMgr::CheckDeclinedNames(GetMainPartOfName(wname, 0), declinedname))
    {
        WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
        data << uint32(1);
        session.SendPacket(&data);
        return;
    }

    // The five declined forms are bound, so whatever escaping the connection needs happens inside
    // the delay thread's own operation, not on the world thread.
    CharacterRepository::SetDeclinedNames(guid.GetCounter(), declinedname.name);

    WorldPacket data(SMSG_SET_PLAYER_DECLINED_NAMES_RESULT, 4 + 8);
    data << uint32(0);                                      // OK
    session.SendPacket(&data);
}

void CharacterCustomizeHandlers::HandleAlterAppearance(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_ALTER_APPEARANCE");

    uint32 Hair, Color, FacialHair, skinTone;
    recv_data >> Hair >> Color >> FacialHair >> skinTone;

    uint32 skinTone_id = -1;
    if (session.GetPlayer()->getRace() == RACE_TAUREN)
    {
        BarberShopStyleEntry const* bs_skinTone = sBarberShopStyleStore.LookupEntry(skinTone);
        if (!bs_skinTone || bs_skinTone->Type != 3 || bs_skinTone->Race != session.GetPlayer()->getRace() || bs_skinTone->Sex != session.GetPlayer()->getGender())
        {
            return;
        }
        skinTone_id = bs_skinTone->Data;
    }

    BarberShopStyleEntry const* bs_hair = sBarberShopStyleStore.LookupEntry(Hair);

    if (!bs_hair || bs_hair->Type != 0 || bs_hair->Race != session.GetPlayer()->getRace() || bs_hair->Sex != session.GetPlayer()->getGender())
    {
        return;
    }

    BarberShopStyleEntry const* bs_facialHair = sBarberShopStyleStore.LookupEntry(FacialHair);

    if (!bs_facialHair || bs_facialHair->Type != 2 || bs_facialHair->Race != session.GetPlayer()->getRace() || bs_facialHair->Sex != session.GetPlayer()->getGender())
    {
        return;
    }

    uint32 Cost = session.GetPlayer()->GetBarberShopCost(bs_hair->Data, Color, bs_facialHair->Data, skinTone_id);

    // 0 - ok
    // 1,3 - not enough money
    // 2 - you have to seat on barber chair
    if (session.GetPlayer()->GetMoney() < Cost)
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(1);                                  // no money
        session.SendPacket(&data);
        return;
    }
    else
    {
        WorldPacket data(SMSG_BARBER_SHOP_RESULT, 4);
        data << uint32(0);                                  // ok
        session.SendPacket(&data);
    }

    session.GetPlayer()->ModifyMoney(-int64(Cost));                     // it isn't free
    session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_AT_BARBER, Cost);

    session.GetPlayer()->SetByteValue(PLAYER_BYTES, 2, uint8(bs_hair->Data));
    session.GetPlayer()->SetByteValue(PLAYER_BYTES, 3, uint8(Color));
    session.GetPlayer()->SetByteValue(PLAYER_BYTES_2, 0, uint8(bs_facialHair->Data));
    if (session.GetPlayer()->getRace() == RACE_TAUREN)
    {
        session.GetPlayer()->SetByteValue(PLAYER_BYTES, 0, uint8(skinTone_id));
    }

    session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_VISIT_BARBER_SHOP, 1);

    session.GetPlayer()->SetStandState(0);                              // stand up
}

/**
 * @brief Starts the asynchronous character customize flow.
 *
 * Nothing is checked or written here: both reads are staged into one holder, and every
 * decision is made in the continuation.
 *
 * @param recv_data The received opcode packet.
 */
void CharacterCustomizeHandlers::HandleCharCustomize(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;
    CharCustomizeRequest request;

    recv_data >> guid;
    recv_data >> request.newname;

    recv_data >> request.gender >> request.skin >> request.hairColor >> request.hairStyle >> request.facialHair >> request.face;

    QueueCharCustomizeReads(session.GetAccountId(), session.GetSessionId(), guid, request);
}

/**
 * @brief Stages the customize handler's two reads.
 *
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character being customized.
 * @param request   The requested name and appearance.
 */
void CharacterCustomizeHandlers::QueueCharCustomizeReads(uint32 accountId, proto::SessionId sessionId, ObjectGuid guid,
                                           CharCustomizeRequest request)
{
    CharacterRepository::QueueCustomizeReads(guid.GetCounter(), [accountId, sessionId, guid, request]
                                             (SqlQueryHolder* h)
                                             {
                                                 CharacterCustomizeHandlers::HandleCharCustomizeCallback(std::unique_ptr<SqlQueryHolder>(h),
                                                                                           accountId, sessionId, guid, request);
                                             });
}

/**
 * @brief Applies the customize, once the character's flags and appearance are known.
 *
 * Every check is the handler's, in the handler's order, with the handler's reply: the
 * at_login flag, then the name's validity, then the reserved list, then whether the name is
 * taken. The cache update sits immediately in front of the write it describes, and the write
 * binds the name instead of escaping it on this thread.
 *
 * @param holder    The two staged reads, answered.
 * @param accountId The requesting account.
 * @param sessionId The session the request arrived on.
 * @param guid      The character being customized.
 * @param request   The requested name and appearance.
 */
void CharacterCustomizeHandlers::HandleCharCustomizeCallback(std::unique_ptr<SqlQueryHolder> holder, uint32 accountId,
                                               proto::SessionId sessionId, ObjectGuid guid,
                                               CharCustomizeRequest request)
{
    WorldSession* session = WorldSession::FindRequesterSession(accountId, sessionId);
    if (!session || !holder)
    {
        return;
    }

    std::string newname = request.newname;

    std::optional<uint32> atLogin = CharacterRepository::CustomizeAtLogin(holder.get());
    if (!atLogin)
    {
        SendCharCustomizeResult(session, CHAR_CREATE_ERROR);
        return;
    }

    uint32 at_loginFlags = *atLogin;

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

    // Immediately before the write it describes; the name it indexes is the one the player
    // typed, which is also the one the statement binds.
    sCharacterCache.UpdateName(guid, newname);

    // The `playerBytes2` value comes out of this holder. No row means no such character, and
    // Player::Customize is skipped, writing nothing.
    if (std::optional<uint32> playerBytes2 = CharacterRepository::CustomizePlayerBytes2(holder.get()))
    {
        Player::Customize(guid, request.gender, request.skin, request.face, request.hairStyle,
                          request.hairColor, request.facialHair, *playerBytes2);
    }

    // The name is bound, not escaped here.
    CharacterRepository::Rename(guid.GetCounter(), newname, uint32(AT_LOGIN_CUSTOMIZE));

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

    sSessionRegistry.InvalidatePlayerData(guid);
}

void CharacterCustomizeHandlers::HandleEquipmentSetSave(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_SAVE_EQUIPMENT_SET");

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

        Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, i);

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

    session.GetPlayer()->SetEquipmentSet(index, eqSet);
}

void CharacterCustomizeHandlers::HandleEquipmentSetDelete(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_EQUIPMENT_SET_DELETE");

    ObjectGuid setGuid;

    recv_data >> setGuid.ReadAsPacked();

    session.GetPlayer()->DeleteEquipmentSet(setGuid.GetRawValue());
}

void CharacterCustomizeHandlers::HandleEquipmentSetUse(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_USE_EQUIPMENT_SET");
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

        Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(itemGuid);

        uint16 dstpos = i | (INVENTORY_SLOT_BAG_0 << 8);

        if (!item)
        {
            Item* uItem = session.GetPlayer()->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (!uItem)
            {
                continue;
            }

            ItemPosCountVec sDest;
            InventoryResult msg = session.GetPlayer()->CanStoreItem(NULL_BAG, NULL_SLOT, sDest, uItem, false);
            if (msg == EQUIP_ERR_OK)
            {
                session.GetPlayer()->RemoveItem(INVENTORY_SLOT_BAG_0, i, true);
                session.GetPlayer()->StoreItem(sDest, uItem, true);
            }
            else
            {
                session.GetPlayer()->SendEquipError(msg, uItem, NULL);
            }

            continue;
        }

        if (item->GetPos() == dstpos)
        {
            continue;
        }

        session.GetPlayer()->SwapItem(item->GetPos(), dstpos);
    }

    WorldPacket data(SMSG_USE_EQUIPMENT_SET_RESULT, 1);
    data << uint8(0);                                       // 4 - equipment swap failed - inventory is full
    session.SendPacket(&data);
}

void CharacterCustomizeHandlers::HandleReorderCharacters(WorldSession& session, WorldPacket& recv_data)
{
    uint32 charCount = recv_data.ReadBits(10);

    if (charCount > sGameConfig.getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM))
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

    CharacterTransaction::Begin();
    for (uint32 i = 0; i < charCount; ++i)
    {
        CharacterRepository::SetSlot(slots[i], guids[i].GetCounter(), session.GetAccountId());
    }
    CharacterTransaction::Commit();
}

void CharacterCustomizeHandlers::HandleSetCurrencyFlags(WorldSession& session, WorldPacket& recv_data)
{
    uint32 currencyId, flags;
    recv_data >> flags >> currencyId;

    DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: currency: %u, flags: %u", currencyId, flags);

    if (flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT)
    {
        DEBUG_LOG("CMSG_SET_CURRENCY_FLAGS: received unknown currency flags 0x%X from player %s account %u for currency %u",
            flags & ~PLAYERCURRENCY_MASK_USED_BY_CLIENT, session.GetPlayer()->GetGuidStr().c_str(), session.GetAccountId(), currencyId);
    }

    flags &= PLAYERCURRENCY_MASK_USED_BY_CLIENT;
    session.GetPlayer()->GetCurrencyMgr().SetFlags(currencyId, uint8(flags));
}

void CharacterCustomizeHandlers::HandleLoadScreen(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_LOAD_SCREEN");
    uint32 mapID;

    recvPacket >> mapID;
    recvPacket.ReadBit();
}
