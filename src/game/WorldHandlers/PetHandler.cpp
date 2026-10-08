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
 * @file PetHandler.cpp
 * @brief Pet interaction opcode handlers
 *
 * This file handles pet-related opcodes including:
 * - CMSG_PET_ACTION: Pet action (attack, follow, stay, etc.)
 * - CMSG_PET_NAME_QUERY: Query pet name
 * - CMSG_PET_ABANDON: Abandon pet
 * - CMSG_PET_RENAME: Rename pet
 * - CMSG_PET_SPELL_AUTOCAST: Toggle autocast
 * - CMSG_PET_CANCEL_AURA: Cancel pet aura
 * - CMSG_PET_STOP_ATTACK: Stop pet attack
 * - CMSG_PET_SET_ACTION: Set pet action bar slot
 *
 * Pets include hunter pets, warlock minions, and temporary summons.
 * Pet actions are validated and synchronized with the owner.
 */

#include "Platform/Define.h"
#include <cstring>
#include <string>
#include <ctime>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Util.h"
#include "Pet.h"
#include "TemporarySummon.h"

/**
 * @brief Handles a client request for a pet name query.
 *
 * @param recv_data The incoming pet name query packet.
 */
void WorldSession::HandlePetNameQueryOpcode(WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetNameQuery. CMSG_PET_NAME_QUERY");

    uint32 petnumber;
    ObjectGuid petguid;

    recv_data >> petnumber;
    recv_data >> petguid;

    SendPetNameQuery(petguid, petnumber);
}

/**
 * @brief Sends pet name data for a specific pet number and guid.
 *
 * @param petguid The pet guid.
 * @param petnumber The pet number.
 */
void WorldSession::SendPetNameQuery(ObjectGuid petguid, uint32 petnumber)
{
    Creature* pet = _player->GetMap()->GetAnyTypeCreature(petguid);
    if (!pet || !pet->GetCharmInfo() || pet->GetCharmInfo()->GetPetNumber() != petnumber)
    {
        WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4 + 1 + 4 + 1));
        data << uint32(petnumber);
        data << uint8(0);
        data << uint32(0);
        data << uint8(0);
        _player->GetSession()->SendPacket(&data);
        return;
    }

    char const* name = pet->GetName();

    // creature pets have localization like other creatures
    if (!pet->GetOwnerGuid().IsPlayer())
    {
        int loc_idx = GetSessionDbLocaleIndex();
        sObjectMgr.GetCreatureLocaleStrings(pet->GetEntry(), loc_idx, &name);
    }

    WorldPacket data(SMSG_PET_NAME_QUERY_RESPONSE, (4 + 4 + strlen(name) + 1));
    data << uint32(petnumber);
    data << name;
    data << uint32(pet->GetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP));

    if (pet->IsPet() && ((Pet*)pet)->GetDeclinedNames())
    {
        data << uint8(1);
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            data << ((Pet*)pet)->GetDeclinedNames()->name[i];
        }
    }
    else
    {
        data << uint8(0);
    }

    _player->GetSession()->SendPacket(&data);
}

/**
 * @brief Handles hunter pet renaming and persists the new name.
 *
 * @param recv_data The incoming pet rename packet.
 */
void WorldSession::HandlePetRename(WorldPacket& recv_data)
{
    DETAIL_LOG("HandlePetRename. CMSG_PET_RENAME");

    ObjectGuid petGuid;
    uint8 isdeclined;

    std::string name;
    DeclinedName declinedname;

    recv_data >> petGuid;
    recv_data >> name;
    recv_data >> isdeclined;

    Pet* pet = _player->GetMap()->GetPet(petGuid);
    // check it!
    if (!pet || pet->getPetType() != HUNTER_PET ||
            !pet->HasByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED) ||
            pet->GetOwnerGuid() != _player->GetObjectGuid() || !pet->GetCharmInfo())
        return;

    PetNameInvalidReason res = ObjectMgr::CheckPetName(name);
    if (res != PET_NAME_SUCCESS)
    {
        SendPetNameInvalid(res, name, NULL);
        return;
    }

    if (sObjectMgr.IsReservedName(name))
    {
        SendPetNameInvalid(PET_NAME_RESERVED, name, NULL);
        return;
    }

    pet->SetName(name);

    if (_player->GetGroup())
    {
        _player->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_NAME);
    }

    pet->RemoveByteFlag(UNIT_FIELD_BYTES_2, 2, UNIT_CAN_BE_RENAMED);

    if (isdeclined)
    {
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            recv_data >> declinedname.name[i];
        }

        std::wstring wname;
        Utf8toWStr(name, wname);
        if (!ObjectMgr::CheckDeclinedNames(GetMainPartOfName(wname, 0), declinedname))
        {
            SendPetNameInvalid(PET_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME, name, &declinedname);
            return;
        }
    }

    // Decoupling D7e: the two tables this handler writes are both in the character's pet
    // cache, so each statement gets its mirror beside it.
    //
    // Decoupling D7i: the six escapes are gone -- five declined-name cases and the name --
    // and every string is a bound parameter (C5). That is also why the cache no longer
    // needs its own copies of them: nothing mutates `name` or `declinedname` any more, so
    // the value the statement binds and the value the cache takes are the same object.
    PlayerPetCache& petCache = _player->GetPetCache();
    const uint32 renamedPetNumber = pet->GetCharmInfo()->GetPetNumber();

    CharacterDatabase.BeginTransaction();
    if (isdeclined)
    {
        static SqlStatementID delDeclinedName;
        SqlStatement remove = CharacterDatabase.CreateStatement(delDeclinedName,
                              "DELETE FROM `character_pet_declinedname` WHERE `owner` = ? AND `id` = ?");
        remove.addUInt32(_player->GetGUIDLow());
        remove.addUInt32(renamedPetNumber);
        remove.Execute();

        static SqlStatementID insDeclinedName;
        SqlStatement insert = CharacterDatabase.CreateStatement(insDeclinedName,
                              "INSERT INTO `character_pet_declinedname` (`id`, `owner`, `genitive`, `dative`, `accusative`, `instrumental`, `prepositional`) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?)");
        insert.addUInt32(renamedPetNumber);
        insert.addUInt32(_player->GetGUIDLow());
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            insert.addString(declinedname.name[i]);
        }
        insert.Execute();

        PetCacheDeclinedName cachedDeclined;
        for (int i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            cachedDeclined.name[i] = declinedname.name[i];
        }
        petCache.SetDeclinedName(renamedPetNumber, cachedDeclined);
    }

    static SqlStatementID updPetName;
    SqlStatement update = CharacterDatabase.CreateStatement(updPetName,
                          "UPDATE `character_pet` SET `name` = ?, `renamed` = '1' WHERE `owner` = ? AND `id` = ?");
    update.addString(name);
    update.addUInt32(_player->GetGUIDLow());
    update.addUInt32(renamedPetNumber);
    update.Execute();

    petCache.SetNameRenamed(renamedPetNumber, name, 1);
    CharacterDatabase.CommitTransaction();

    pet->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL)));
}

/**
 * @brief Handles abandoning or dismissing a pet or charm.
 *
 * @param recv_data The incoming pet abandon packet.
 */
void WorldSession::HandlePetAbandon(WorldPacket& recv_data)
{
    ObjectGuid guid;
    recv_data >> guid;                                      // pet guid

    DETAIL_LOG("HandlePetAbandon. CMSG_PET_ABANDON pet guid is %s", guid.GetString().c_str());

    if (!_player->IsInWorld())
    {
        return;
    }

    // pet/charmed
    if (Creature* pet = _player->GetMap()->GetAnyTypeCreature(guid))
    {
        if (pet->IsPet())
        {
            ((Pet*)pet)->Unsummon(PET_SAVE_AS_DELETED, _player);
        }
        else if (pet->GetObjectGuid() == _player->GetCharmGuid())
        {
            _player->Uncharm();
        }
    }
}

/**
 * @brief Sends a pet-name-invalid response to the client.
 *
 * @param error The invalid-name error code.
 * @param name The rejected name.
 */
void WorldSession::SendPetNameInvalid(uint32 error, const std::string& name, DeclinedName* declinedName)
{
    WorldPacket data(SMSG_PET_NAME_INVALID, 4 + name.size() + 1 + 1);
    data << uint32(error);
    data << name;
    if (declinedName)
    {
        data << uint8(1);
        for (uint32 i = 0; i < MAX_DECLINED_NAME_CASES; ++i)
        {
            data << declinedName->name[i];
        }
    }
    else
    {
        data << uint8(0);
    }
    SendPacket(&data);
}

void WorldSession::HandlePetLearnTalent(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_PET_LEARN_TALENT");

    ObjectGuid guid;
    uint32 talent_id, requested_rank;
    recv_data >> guid >> talent_id >> requested_rank;

    _player->LearnPetTalent(guid, talent_id, requested_rank);
    _player->SendTalentsInfoData(true);
}

void WorldSession::HandleLearnPreviewTalentsPet(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LEARN_PREVIEW_TALENTS_PET");

    ObjectGuid guid;
    recv_data >> guid;

    uint32 talentsCount;
    recv_data >> talentsCount;

    uint32 talentId, talentRank;

    for (uint32 i = 0; i < talentsCount; ++i)
    {
        recv_data >> talentId >> talentRank;

        _player->LearnPetTalent(guid, talentId, talentRank);
    }

    _player->SendTalentsInfoData(true);
}

void WorldSession::HandleDismissCritter(WorldPacket& recvData)
{
    ObjectGuid guid;
    recvData >> guid;

    DEBUG_LOG("WORLD: Received CMSG_DISMISS_CRITTER for %s", guid.GetString().c_str());
    Unit* pet = _player->GetMap()->GetAnyTypeCreature(guid);
    if (!pet)
    {
        DEBUG_LOG("Vanitypet %s does not exist - Player %s (guid %u, account: %u) attempted to dismiss it (possibly lagged out)",
        guid.GetString().c_str(), GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), GetAccountId());
        return;
    }

    if (_player->GetCritterGuid() == guid)
    {
        if (pet->GetTypeId() == TYPEID_UNIT && ((Creature*)pet)->IsTemporarySummon())
        {
            ((TemporarySummon*)pet)->UnSummon();
        }
    }
}
