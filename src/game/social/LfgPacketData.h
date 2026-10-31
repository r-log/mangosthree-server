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

#ifndef MANGOS_LFGPACKETDATA_H
#define MANGOS_LFGPACKETDATA_H

/**
 * @file LfgPacketData.h
 * @brief The plain data the dungeon finder fills for its outgoing packets.
 *
 * LFGMgr fills these and hands them to the session, whose builders in
 * LFGPackets.h write them. Nothing here knows about Player, Group, or
 * WorldSession.
 */

#include "Platform/Define.h"

#include <array>
#include <string>
#include <vector>

/// 4.3.4 ticket kind carried by RideTicket::type. Value 2 is intentionally
/// absent -- confirmed against tc-preservation's RideType, not a typo.
enum RideType
{
    RIDE_TYPE_NONE          = 0,
    RIDE_TYPE_BATTLEGROUNDS = 1,
    RIDE_TYPE_LFG           = 3
};

/// 4.3.4 queue ticket: identifies a dungeon-finder request. The bit-packed
/// packets never write it as a block; each interleaves its four fields with
/// its own bit and byte runs.
struct RideTicket
{
    int32 time = 0;                ///< queue join time
    RideType type = RIDE_TYPE_NONE;
    uint32 id = 0;                 ///< queue / ticket id
    uint64 requesterGuid = 0;      ///< player or group guid, raw
};

/// Wire layout for a single dungeon-finder lock reason. Shared by
/// SMSG_LFG_PLAYER_INFO and SMSG_LFG_PARTY_INFO, both of which write
/// Slot, Reason, SubReason1, SubReason2 via the client's common lock-list
/// reader (sub_1407021A0). SMSG_LFG_JOIN_RESULT uses a different
/// field order and must not reuse this struct.
struct LFGLockedDungeon
{
    uint32 slot = 0;
    uint32 reason = 0;
    uint32 subReason1 = 0;        ///< required item level, for gear locks
    uint32 subReason2 = 0;        ///< current item level, for gear locks
};

/// A single dungeon-finder reward item or currency payout.
struct LFGRewardItem
{
    uint32 id = 0;                ///< item id, or currency id when isCurrency
    uint32 displayId = 0;         ///< 0 for currency
    uint32 quantity = 0;
    bool isCurrency = false;
};

namespace LFGPackets
{
    // SMSG_LFG_ROLE_CHECK_UPDATE

    /// One role-check participant. The leader MUST be first in
    /// RoleCheckUpdate::members -- the client has no other way to tell.
    struct LFGRoleCheckMember
    {
        uint64 guid = 0;           ///< raw, not packed
        bool roleCheckComplete = false;
        uint32 rolesDesired = 0;   ///< bit1 tank, bit2 healer, bit3 dps
        uint8 level = 0;
    };

    struct RoleCheckUpdate
    {
        uint32 status = 0;         ///< LfgRoleCheckState
        bool isBeginning = false;  ///< nonzero shows the role-check UI
        std::vector<uint32> dungeons;               ///< resolved dungeon entries
        std::vector<LFGRoleCheckMember> members;    ///< leader first
    };

    // SMSG_LFG_PLAYER_REWARD

    struct PlayerReward
    {
        uint32 queuedSlot = 0;       ///< random dungeon entry
        uint32 actualSlot = 0;       ///< actual dungeon entry
        uint32 rewardMoney = 0;
        uint32 addedXp = 0;
        std::vector<LFGRewardItem> items;
    };

    // SMSG_LFG_PLAYER_INFO

    /// One Call to Arms shortage-role reward tier. When roleMask is 0 the
    /// client's reader skips the rest of the iteration entirely (confirmed
    /// at Wow-64.c:1829314) -- money/xp/items below must not be written for
    /// a zero-mask tier. This is the opposite of TrinityCore, which always
    /// writes three placeholder zero fields; following TC's shape here
    /// would desynchronise the stream.
    struct LFGShortageReward
    {
        uint32 roleMask = 0;         ///< 0 => tier absent, nothing else written
        uint32 money = 0;
        uint32 xp = 0;
        std::vector<LFGRewardItem> items;
    };

    /// One random-dungeon reward entry (client struct LFGEntryInfo).
    struct LFGRandomDungeonEntry
    {
        uint32 slot = 0;
        bool firstReward = false;
        uint32 completionQuantity = 0;
        uint32 completionLimit = 0;
        uint32 completionCurrencyId = 0;
        uint32 specificQuantity = 0;
        uint32 specificLimit = 0;
        uint32 overallQuantity = 0;
        uint32 overallLimit = 0;
        uint32 purseWeeklyQuantity = 0;
        uint32 purseWeeklyLimit = 0;
        uint32 purseQuantity = 0;
        uint32 purseLimit = 0;
        uint32 quantity = 0;
        uint32 completedMask = 0;
        bool shortageEligible = false;
        std::array<LFGShortageReward, 3> shortageTiers;   ///< severity: rare, uncommon, plentiful
        uint32 rewardMoney = 0;
        uint32 rewardXp = 0;
        std::vector<LFGRewardItem> items;
    };

    // SMSG_LFG_QUEUE_STATUS

    struct QueueStatus
    {
        RideTicket ticket;
        uint32 slot = 0;
        int32 avgWaitTimeMe = 0;
        int32 avgWaitTime = 0;
        std::array<int32, 3> avgWaitTimeByRole{};
        std::array<uint8, 3> lastNeeded{};
        uint32 queuedTime = 0;
    };

    // SMSG_LFG_UPDATE_STATUS

    struct UpdateStatus
    {
        RideTicket ticket;
        uint8 reason = 0;
        std::vector<uint32> slots;
        std::string comment;          ///< wire length is a 9-bit count, max 511
        std::array<uint8, 3> needs{}; ///< tank/healer/damage still wanted
        bool isParty = false;
        bool joined = false;
        bool lfgJoined = false;
        bool queued = false;
    };

    /// Joined/Queued/LfgJoined wire bits for SMSG_LFG_UPDATE_STATUS, derived
    /// from the update reason and the player's state. The numbers are LFGMgr.h's
    /// LfgUpdateType and LFGState values, which this header cannot include.
    struct UpdateFlags
    {
        bool joined = false;
        bool queued = false;
        bool lfgJoined = false;
    };

    inline UpdateFlags DeriveUpdateFlags(uint8 reason, uint8 state)
    {
        UpdateFlags flags;
        switch (reason)
        {
            case 24:                              // JOIN_INITIAL
            case 3:                               // JOIN_RAIDBROWSER
            case 14:                              // PROPOSAL_BEGIN
                flags.joined = true;
                break;
            case 6:                               // JOIN (rolecheck begins)
            case 13:                              // ADDED_TO_QUEUE
                flags.joined = true;
                flags.queued = true;
                break;
            case 15:                              // UPDATE_STATUS
                flags.joined = state != 1 && state != 0 && state != 5 && state != 6;
                flags.queued = state == 2 || state == 7;
                break;
            default:
                break;
        }

        flags.lfgJoined = reason != 8;            // REMOVED_FROM_QUEUE
        return flags;
    }

    // SMSG_LFG_JOIN_RESULT

    /// One blacklisted-dungeon lock reason inside a JoinResultBlacklist
    /// entry. Field order (Reason, SubReason1, SubReason2, Slot) is not
    /// the same as LFGLockedDungeon's (Slot, Reason, SubReason1,
    /// SubReason2).
    struct JoinResultBlacklistSlot
    {
        uint32 slot = 0;
        uint32 reason = 0;
        uint32 subReason1 = 0;
        uint32 subReason2 = 0;
    };

    struct JoinResultBlacklist
    {
        uint64 guid = 0;              ///< player or group guid, packed
        std::vector<JoinResultBlacklistSlot> slots;
    };

    // SMSG_LFG_PROPOSAL_UPDATE

    struct ProposalUpdatePlayer
    {
        uint32 roles = 0;
        bool me = false;
        bool sameParty = false;
        bool myParty = false;
        bool responded = false;
        bool accepted = false;
    };

    struct ProposalUpdate
    {
        RideTicket ticket;
        uint64 instanceId = 0;        ///< dungeon instance map guid, packed
        uint32 proposalId = 0;
        uint32 slot = 0;
        int8 state = 0;
        uint32 completedMask = 0;
        bool proposalSilent = false;
        std::vector<ProposalUpdatePlayer> players;
    };
}

#endif
