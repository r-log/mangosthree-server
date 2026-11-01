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
 * @file LFGHandler.cpp
 * @brief Defines the WorldSession members that send the Dungeon Finder packets, called by
 *        LFGMgr and LfgHandlers. Packet layouts live in LFGPackets.h; queue logic in LFGMgr.
 */

#include <string>
#include <vector>
#include "WorldSession.h"
#include "Group.h"
#include "LFGMgr.h"
#include "session/packets/social/LFGPackets.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#include "PlayerRegistry.h"

void WorldSession::SendLfgJoinResult(LfgJoinResult result, uint8 resultDetail,
                                     RideTicket const& ticket,
                                     std::vector<LFGPackets::JoinResultBlacklist> const& blacklist)
{
    LFGPackets::JoinResult data;
    data.ticket = ticket;
    data.result = uint8(result);
    data.resultDetail = resultDetail;
    data.blacklist = blacklist;

    WorldPacket packet(SMSG_LFG_JOIN_RESULT, 32);
    if (LFGPackets::BuildJoinResult(packet, data))
    {
        SendPacket(&packet);
    }
}

void WorldSession::SendLfgUpdateStatus(LFGPackets::UpdateStatus const& status)
{
    WorldPacket packet(SMSG_LFG_UPDATE_STATUS, 32 + status.slots.size() * 4);
    if (LFGPackets::BuildUpdateStatus(packet, status))
    {
        SendPacket(&packet);
    }
}

void WorldSession::SendLfgQueueStatus(LFGPackets::QueueStatus const& status)
{
    WorldPacket packet(SMSG_LFG_QUEUE_STATUS, 47);
    LFGPackets::BuildQueueStatus(packet, status);
    SendPacket(&packet);
}

void WorldSession::SendLfgRoleCheckUpdate(LFGPackets::RoleCheckUpdate const& roleCheck)
{
    WorldPacket packet(SMSG_LFG_ROLE_CHECK_UPDATE,
                       7 + roleCheck.dungeons.size() * 4 + roleCheck.members.size() * 14);
    if (LFGPackets::BuildRoleCheckUpdate(packet, roleCheck))
    {
        SendPacket(&packet);
    }
}

void WorldSession::SendLfgRoleChosen(uint64 rawGuid, uint32 roles)
{
    WorldPacket packet(SMSG_ROLE_CHOSEN, 13);
    LFGPackets::BuildRoleChosen(packet, rawGuid, roles);
    SendPacket(&packet);
}

void WorldSession::SendLfgProposalUpdate(LFGPackets::ProposalUpdate const& proposal)
{
    WorldPacket packet(SMSG_LFG_PROPOSAL_UPDATE, 36 + proposal.players.size() * 6);
    if (LFGPackets::BuildProposalUpdate(packet, proposal))
    {
        SendPacket(&packet);
    }
}

void WorldSession::SendLfgTeleportError(uint8 error)
{
    WorldPacket data(SMSG_LFG_TELEPORT_DENIED, 4);
    LFGPackets::BuildTeleportDenied(data, uint32(error));
    SendPacket(&data);
}

void WorldSession::SendLfgPlayerReward(LFGPackets::PlayerReward const& reward)
{
    DEBUG_LOG("SMSG_LFG_PLAYER_REWARD");

    WorldPacket data(SMSG_LFG_PLAYER_REWARD, 42);
    if (!LFGPackets::BuildPlayerReward(data, reward))
    {
        sLog.outError("SMSG_LFG_PLAYER_REWARD: %s has %u reward items, wire holds a uint8",
                      _player ? _player->GetGuidStr().c_str() : "<no player>",
                      uint32(reward.items.size()));
        return;
    }

    SendPacket(&data);
}

/// Sends SMSG_LFG_BOOT_PROPOSAL_UPDATE for one recipient.
///
/// timeLeft is the seconds until the boot vote's deadline, floored at 0.
void WorldSession::SendLfgBootProposalUpdate(LFGBoot const& boot, uint32 votesNeeded)
{
    DEBUG_LOG("SMSG_LFG_BOOT_PROPOSAL_UPDATE");

    ObjectGuid plrGuid = GetPlayer()->GetObjectGuid();

    proposalAnswerMap::const_iterator ansItr = boot.answers.find(plrGuid);
    LFGProposalAnswer plrAnswer = (ansItr != boot.answers.end()) ? ansItr->second : LFG_ANSWER_PENDING;

    uint32 totalVotes = 0, bootVotes = 0;
    for (proposalAnswerMap::const_iterator it = boot.answers.begin(); it != boot.answers.end(); ++it)
    {
        if (it->second != LFG_ANSWER_PENDING)
        {
            ++totalVotes;
            if (it->second == LFG_ANSWER_AGREE)
            {
                ++bootVotes;
            }
        }
    }

    time_t const now = time(NULL);
    time_t const deadline = boot.startTime + LFG_TIME_BOOT;
    uint32 const timeLeft = (deadline > now) ? uint32(deadline - now) : 0;

    LFGPackets::BootProposalUpdate data;
    data.voteInProgress = boot.inProgress;
    data.votePassed = boot.votePassed;
    data.myVoteCompleted = plrAnswer != LFG_ANSWER_PENDING;
    data.myVote = plrAnswer == LFG_ANSWER_AGREE;
    data.target = boot.playerVotedOn.GetRawValue();
    data.totalVotes = totalVotes;
    data.bootVotes = bootVotes;
    data.timeLeft = timeLeft;
    data.votesNeeded = votesNeeded;
    data.reason = boot.reason;

    WorldPacket packet(SMSG_LFG_BOOT_PROPOSAL_UPDATE, 28 + boot.reason.length() + 1);
    if (LFGPackets::BuildBootProposalUpdate(packet, data))
    {
        SendPacket(&packet);
    }
    else
    {
        sLog.outError("WorldSession::SendLfgBootProposalUpdate: reason too long (%u bytes), packet not sent",
                      uint32(boot.reason.length()));
    }
}

/// SMSG_LFG_OFFER_CONTINUE: offers the group a backfill for its dungeon.
void WorldSession::SendLfgOfferContinue(uint32 dungeonEntry)
{
    DEBUG_LOG("SMSG_LFG_OFFER_CONTINUE %u", dungeonEntry);

    WorldPacket data(SMSG_LFG_OFFER_CONTINUE, 4);
    LFGPackets::BuildOfferContinue(data, dungeonEntry);
    SendPacket(&data);
}

/// Builds and sends SMSG_LFG_PLAYER_INFO for the requesting player. Drives
/// the padlocks in the Dungeon Finder's own list (LFG_LOCK_INFO_RECEIVED)
/// and the reward panel: randomDungeons is what puts "Random Dungeon" in
/// the "Type:" dropdown at all, and carries the money, experience,
/// currency and remaining-completions the panel draws beside it.
void WorldSession::SendLfgPlayerLockInfo()
{
    Player* player = GetPlayer();

    LFGPackets::PlayerInfo info;
    sLFGMgr.BuildRandomDungeonRewards(player, info.randomDungeons);
    info.lockedDungeons = sLFGMgr.GetPlayerLockList(player);

    WorldPacket data(SMSG_LFG_PLAYER_INFO,
                     5 + info.randomDungeons.size() * 80 + info.lockedDungeons.size() * 16);
    if (LFGPackets::BuildPlayerInfo(data, info))
    {
        SendPacket(&data);
    }
}

/// Builds and sends SMSG_LFG_PARTY_INFO for the requesting player's group.
/// Self is skipped; with no group this sends a valid,
/// empty member list.
void WorldSession::SendLfgPartyLockInfo()
{
    Player* player = GetPlayer();

    LFGPackets::PartyInfo info;

    if (Group* group = player->GetGroup())
    {
        ObjectGuid selfGuid = player->GetObjectGuid();

        for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* member = itr->getSource();
            if (!member || member->GetObjectGuid() == selfGuid)
            {
                continue;
            }

            LFGPackets::LFGPartyInfoMember memberInfo;
            memberInfo.guid = member->GetObjectGuid().GetRawValue();
            memberInfo.lockedDungeons = sLFGMgr.GetPlayerLockList(member);

            info.members.push_back(memberInfo);
        }
    }

    WorldPacket data(SMSG_LFG_PARTY_INFO, 1 + info.members.size() * 12);
    if (LFGPackets::BuildPartyInfo(data, info))
    {
        SendPacket(&data);
    }
}


