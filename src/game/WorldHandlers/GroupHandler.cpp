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
 * @file GroupHandler.cpp
 * @brief Group/party opcode handlers
 *
 * Group operations require proper permission checks and state validation.
 */

#include "Platform/Define.h"
#include <string>
#include <cstring>
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "AchievementMgr.h"
#include "Player.h"
#include "spells/auras/SpellAuras.h"
#include "Group.h"
#include "LFGMgr.h"                                         // LFGRoles: PLAYER_ROLE_*
#include "SocialMgr.h"
#include "Util.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "Vehicle.h"
#include "PlayerRegistry.h"

/* differeces from off:
    -you can uninvite yourself - is is useful
    -you can accept invitation even if leader went offline
*/
/* todo:
    -group_destroyed msg is sent but not shown
    -reduce xp gaining when in raid group
    -quest sharing has to be corrected
    -FIX sending PartyMemberStats
*/

/**
 * @brief Sends a party operation result packet to the client.
 *
 * @param operation The party operation being reported.
 * @param member The related member name.
 * @param res The result code to send.
 * @param val LFD cooldown seconds (ERR_PARTY_LFG_BOOT_COOLDOWN_S / _NOT_ELIGIBLE_S); 0 otherwise.
 * @param guid Victim guid for ERR_VOTE_KICK_REASON_NEEDED; empty otherwise.
 */
void WorldSession::SendPartyResult(PartyOperation operation, const std::string& member,
                                   PartyResult res, uint32 val, ObjectGuid guid)
{
    WorldPacket data(SMSG_PARTY_COMMAND_RESULT, 4 + member.size() + 1 + 4 + 4 + 8);
    data << uint32(operation);
    data << member;                                         // max len 48
    data << uint32(res);
    data << uint32(val);                                    // LFD cooldown related (used with ERR_PARTY_LFG_BOOT_COOLDOWN_S and ERR_PARTY_LFG_BOOT_NOT_ELIGIBLE_S)
    data << guid;                                            // if result == 27 (ERR_VOTE_KICK_REASON_NEEDED), then it's guid of player being kicked (member's guid)

    SendPacket(&data);
}

void WorldSession::SendGroupInvite(Player* player, bool alreadyInGroup /*= false*/)
{
    WorldPacket data(SMSG_GROUP_INVITE, 21);                // guess size
    data.WriteBit(0);
    data.WriteGuidMask<0, 3, 2>(player->GetObjectGuid());
    data.WriteBit(!alreadyInGroup);
    data.WriteGuidMask<6, 5>(player->GetObjectGuid());
    data.WriteBits(0, 9);                                   // realm name length
    data.WriteGuidMask<4>(player->GetObjectGuid());
    data.WriteBits(strlen(player->GetName()), 7);
    data.WriteBits(0, 24);                                  // count
    data.WriteBit(0);
    data.WriteGuidMask<1, 7>(player->GetObjectGuid());

    data.WriteGuidBytes<1, 4>(player->GetObjectGuid());
    data << uint32(GameTime::GetGameTimeMS());
    data << uint32(0) << uint32(0);
    data.WriteGuidBytes<6, 0, 2, 3>(player->GetObjectGuid());
    // for(int i = 0; i < count; ++i)
    //    data << uint32(0);
    data.WriteGuidBytes<5>(player->GetObjectGuid());
    data.WriteGuidBytes<7>(player->GetObjectGuid());
    data.append(player->GetName(), strlen(player->GetName()));
    data << uint32(0);

    SendPacket(&data);
}

/**
 * @brief Builds a party member stats update packet.
 *
 * @param player The player whose stats are being serialized.
 * @param data The packet receiving the serialized fields.
 */
void WorldSession::BuildPartyMemberStatsChangedPacket(Player* player, WorldPacket* data)
{
    uint32 mask = player->GetGroupUpdateFlag();

    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)                // if update power type, update current/max power also
    {
        mask |= (GROUP_UPDATE_FLAG_CUR_POWER | GROUP_UPDATE_FLAG_MAX_POWER);
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)            // same for pets
    {
        mask |= (GROUP_UPDATE_FLAG_PET_CUR_POWER | GROUP_UPDATE_FLAG_PET_MAX_POWER);
    }

    uint32 byteCount = 0;
    for (int i = 1; i < GROUP_UPDATE_FLAGS_COUNT; ++i)
        if (mask & (1 << i))
        {
            byteCount += GroupUpdateLength[i];
        }

    data->Initialize(SMSG_PARTY_MEMBER_STATS, 8 + 4 + byteCount);
    *data << player->GetPackGUID();
    *data << uint32(mask);

    if (mask & GROUP_UPDATE_FLAG_STATUS)
    {
        if (player->IsPvP())
        {
            *data << uint16(MEMBER_STATUS_ONLINE | MEMBER_STATUS_PVP);
        }
        else
        {
            *data << uint16(MEMBER_STATUS_ONLINE);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_HP)
    {
        *data << uint32(player->GetHealth());
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_HP)
    {
        *data << uint32(player->GetMaxHealth());
    }

    Powers powerType = player->GetPowerType();
    if (mask & GROUP_UPDATE_FLAG_POWER_TYPE)
    {
        *data << uint8(powerType);
    }

    if (mask & GROUP_UPDATE_FLAG_CUR_POWER)
    {
        *data << uint16(player->GetPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_MAX_POWER)
    {
        *data << uint16(player->GetMaxPower(powerType));
    }

    if (mask & GROUP_UPDATE_FLAG_LEVEL)
    {
        *data << uint16(player->getLevel());
    }

    if (mask & GROUP_UPDATE_FLAG_ZONE)
    {
        *data << uint16(player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z()));
    }

    if (mask & GROUP_UPDATE_FLAG_UNK)
    {
        *data << uint16(0);
    }

    if (mask & GROUP_UPDATE_FLAG_POSITION)
    {
        *data << uint16(player->Where().X()) << uint16(player->Where().Y()) << uint16(player->Where().Z());
    }

    if (mask & GROUP_UPDATE_FLAG_AURAS)
    {
        *data << uint8(0);                  // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
        const uint64& auramask = player->GetAuraUpdateMask();
        *data << uint64(auramask);
        *data << uint32(MAX_AURAS);         // server sends here number of visible auras, but client checks
                                            // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
        for (uint32 i = 0; i < MAX_AURAS; ++i)
        {
            if (auramask & (uint64(1) << i))
            {
                if (SpellAuraHolder* holder = player->GetVisibleAura(i))
                {
                    *data << uint32(holder->GetId());
                    *data << uint16(holder->GetAuraFlags());
                    if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
                    {
                        for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                        {
                            if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                            {
                                *data << int32(aura->GetModifier()->m_amount);
                            }
                            else
                            {
                                *data << int32(0);
                            }
                        }
                    }
                }
                else
                {
                    *data << uint32(0);
                    *data << uint16(0);
                }
            }
        }
    }

    Pet* pet = player->GetPet();
    if (mask & GROUP_UPDATE_FLAG_PET_GUID)
    {
        *data << (pet ? pet->GetObjectGuid() : ObjectGuid());
    }

    if (mask & GROUP_UPDATE_FLAG_PET_NAME)
    {
        if (pet)
        {
            *data << pet->GetName();
        }
        else
        {
            *data << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MODEL_ID)
    {
        if (pet)
        {
            *data << uint16(pet->GetDisplayId());
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_HP)
    {
        if (pet)
        {
            *data << uint32(pet->GetHealth());
        }
        else
        {
            *data << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_HP)
    {
        if (pet)
        {
            *data << uint32(pet->GetMaxHealth());
        }
        else
        {
            *data << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_POWER_TYPE)
    {
        if (pet)
        {
            *data << uint8(pet->GetPowerType());
        }
        else
        {
            *data << uint8(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_CUR_POWER)
    {
        if (pet)
        {
            *data << uint16(pet->GetPower(pet->GetPowerType()));
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_MAX_POWER)
    {
        if (pet)
        {
            *data << uint16(pet->GetMaxPower(pet->GetPowerType()));
        }
        else
        {
            *data << uint16(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_PET_AURAS)
    {
        if (pet)
        {
            *data << uint8(0);              // if true, client clears all auras that are not in auramask and whose index is lower amount sent below
            const uint64& auramask = pet->GetAuraUpdateMask();
            *data << uint64(auramask);
            *data << uint32(MAX_AURAS);     // server sends here number of visible auras, but client checks
                                            // if aura is in auramask, so it seems no difference if there will be MAX_AURAS
            for (uint32 i = 0; i < MAX_AURAS; ++i)
            {
                if (auramask & (uint64(1) << i))
                {
                    if (SpellAuraHolder* holder = pet->GetVisibleAura(i))
                    {
                        *data << uint32(holder->GetId());
                        *data << uint16(holder->GetAuraFlags());
                        if (holder->GetAuraFlags() & AFLAG_EFFECT_AMOUNT_SEND)
                        {
                            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                            {
                                if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                                {
                                    *data << int32(aura->GetModifier()->m_amount);
                                }
                                else
                                {
                                    *data << int32(0);
                                }
                            }
                        }
                    }
                    else
                    {
                        *data << uint32(0);
                        *data << uint16(0);
                    }
                }
            }
        }
        else
        {
            *data << uint8(0);
            *data << uint64(0);
            *data << uint32(0);
        }
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        *data << int32(0);
    }

    if (mask & GROUP_UPDATE_FLAG_PHASE)
    {
        *data << uint32(8);
        *data << uint32(0);
        *data << uint8(0);
    }

    if (mask & GROUP_UPDATE_FLAG_VEHICLE_SEAT)
    {
        if (player->GetTransportInfo())
        {
            *data << uint32(((Unit*)player->GetTransportInfo()->GetTransport())->GetVehicleInfo()->GetVehicleEntry()->SeatID[player->GetTransportInfo()->GetTransportSeat()]);
        }
        else
        {
            *data << uint32(0);
        }
    }
}

