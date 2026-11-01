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

#include "session/handlers/social/LfgHandlers.h"

#include <set>
#include "session/WorldSession.h"
#include "data/dbc/DBCStores.h"
#include "social/Group.h"
#include "social/LFGMgr.h"
#include "session/packets/social/LFGPackets.h"
#include "Log/Log.h"
#include "entities/player/Player.h"
#include "WorldPacket.h"
#include "data/GameConfig.h"

void LfgHandlers::HandleLfgJoin(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_LFG_JOIN");

    LFGPackets::JoinRequest request;
    LFGPackets::ReadJoin(recv_data, request);

    if (!sGameConfig.getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        return;
    }

    Player* player = session.GetPlayer();

    // The client only greys the button; refuse silently, exactly as retail
    // does -- there is no "leader only" message in GlobalStrings.
    if (!LFGMgr::IsEmpowered(player))
    {
        return;
    }

    std::set<uint32> dungeons;
    for (uint32 slot : request.slots)
    {
        uint32 dungeonId = slot & 0x00FFFFFF;
        if (sLfgDungeonsStore.LookupEntry(dungeonId))
        {
            dungeons.insert(dungeonId);
        }
    }

    if (dungeons.empty())
    {
        return;
    }

    sLFGMgr.JoinLFG(request.roles, dungeons, request.comment, player);
}

void LfgHandlers::HandleLfgLeave(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_LFG_LEAVE");

    LFGPackets::LeaveRequest request;
    LFGPackets::ReadLeave(recv_data, request);

    Player* player = session.GetPlayer();
    Group* group = player->GetGroup();

    // The client only greys the button; refuse silently, exactly as retail
    // does -- there is no "leader only" message in GlobalStrings. The ticket
    // guid the client echoes is deliberately ignored in favour of the
    // session's own identity.
    if (!LFGMgr::IsEmpowered(player))
    {
        return;
    }

    sLFGMgr.LeaveLFG(player, group != nullptr);
}

void LfgHandlers::HandleLfgSetRoles(WorldSession& session, WorldPacket& recv_data)
{
    LFGPackets::SetRolesRequest request;
    LFGPackets::ReadSetRoles(recv_data, request);

    Player* player = session.GetPlayer();
    Group* group = player->GetGroup();
    if (!group)
    {
        return;                          // solo role choices are client-local
    }

    uint32 roles = request.rolesDesired;
    if (!LFGMgr::CanPerformSelectedRoles(player->getClass(), uint8(roles)))
    {
        roles = 0;                       // treated as "no role" => check fails
    }

    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_LFG_SET_ROLES roles %u", roles);
    sLFGMgr.PerformRoleCheck(player, group, uint8(roles));
}

void LfgHandlers::HandleLfgGetStatus(WorldSession& session, WorldPacket& /*recv_data*/)
{
    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_LFG_GET_STATUS");

    if (!sGameConfig.getConfig(CONFIG_BOOL_LFG_ENABLE))
    {
        return;
    }

    sLFGMgr.SendStatusUpdate(session.GetPlayer());
}

void LfgHandlers::HandleLfgProposalResult(WorldSession& session, WorldPacket& recv_data)
{
    LFGPackets::ProposalResult request;
    LFGPackets::ReadProposalResult(recv_data, request);

    DEBUG_FILTER_LOG(LOG_FILTER_LFG,
        "CMSG_LFG_PROPOSAL_RESULT proposal %u accepted %u",
        request.proposalId, uint32(request.accepted));

    // The echoed ticket/instance guid are informational; the
    // session's own player is the identity that answers.
    sLFGMgr.ProposalUpdate(request.proposalId, session.GetPlayer()->GetObjectGuid(),
                           request.accepted);
}

void LfgHandlers::HandleLfgTeleport(WorldSession& session, WorldPacket& recv_data)
{
    LFGPackets::TeleportRequest request;
    LFGPackets::ReadTeleport(recv_data, request);

    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_LFG_TELEPORT out %u",
        uint32(request.teleportOut));

    // No LFG.Enable gate: without LFG no group status exists and the
    // manager denies with INVALID_LOCATION on its own.
    sLFGMgr.TeleportPlayer(session.GetPlayer(), request.teleportOut, false);
}

void LfgHandlers::HandleSetLfgComment(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_FILTER_LOG(LOG_FILTER_LFG, "CMSG_SET_LFG_COMMENT");

    LFGPackets::SetCommentRequest request;
    LFGPackets::ReadSetComment(recv_data, request);

    sLFGMgr.SetPlayerComment(session.GetPlayer()->GetObjectGuid(), request.comment);
}

/// Fires on Dungeon Finder panel open (LFDFrame.xml's OnShow calls both
/// RequestLFDPlayerLockInfo and RequestLFDPartyLockInfo, each a separate
/// CMSG_LFG_LOCK_INFO_REQUEST distinguished by this one bit).
void LfgHandlers::HandleLfgLockInfoRequest(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_LOCK_INFO_REQUEST");

    LFGPackets::LockInfoRequest request;
    LFGPackets::ReadLockInfoRequest(recv_data, request);

    if (request.player)
    {
        session.SendLfgPlayerLockInfo();
    }
    else
    {
        session.SendLfgPartyLockInfo();
    }
}

/// CMSG_LFG_SET_BOOT_VOTE: records this player's answer to a boot vote.
void LfgHandlers::HandleLfgSetBootVote(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_LFG_SET_BOOT_VOTE");

    LFGPackets::SetBootVoteRequest request;
    LFGPackets::ReadSetBootVote(recv_data, request);

    sLFGMgr.CastVote(session.GetPlayer(), request.vote);
}
