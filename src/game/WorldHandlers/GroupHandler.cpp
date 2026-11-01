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

