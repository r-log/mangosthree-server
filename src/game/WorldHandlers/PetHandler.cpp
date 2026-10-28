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
 * @brief Defines two WorldSession members: SendPetNameQuery, which answers a pet name query, and
 * SendPetNameInvalid, which tells the client a pet name was refused.
 */

#include "Platform/Define.h"
#include <cstring>
#include <string>
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "Pet.h"

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
