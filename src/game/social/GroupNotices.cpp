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

#include "social/GroupNotices.h"

#include <atomic>

namespace
{
    std::atomic<GroupNotices*> s_groupNoticesInstalled(NULL);

    GroupNotices& BaseGroupNotices()
    {
        static GroupNotices notices;
        return notices;
    }
}

GroupNotices::~GroupNotices()
{
}

void GroupNotices::RaidMarkersChanged(Group& /*group*/)
{
}

void GroupNotices::LeaderChanged(Group& /*group*/, std::string const& /*name*/)
{
}

void GroupNotices::Uninvited(Player& /*player*/)
{
}

void GroupNotices::Destroyed(Player& /*player*/)
{
}

void GroupNotices::NoGroup(Player& /*player*/)
{
}

void GroupNotices::LootStartRoll(Player& /*player*/, Roll const& /*roll*/, uint32 /*countDown*/, uint32 /*mapId*/, RollVoteMask /*mask*/)
{
}

void GroupNotices::LootRoll(Group& /*group*/, Roll const& /*roll*/, ObjectGuid /*target*/, uint32 /*rollNumber*/, uint8 /*rollType*/)
{
}

void GroupNotices::LootRollWon(Group& /*group*/, Roll const& /*roll*/, ObjectGuid /*target*/, uint32 /*rollNumber*/, RollVote /*rollType*/)
{
}

void GroupNotices::LootAllPassed(Group& /*group*/, Roll const& /*roll*/)
{
}

void GroupNotices::MasterLootList(std::vector<ObjectGuid> const& /*candidates*/, std::vector<Player*> const& /*recipients*/)
{
}

void GroupNotices::TargetIconSet(Group& /*group*/, ObjectGuid /*who*/, uint8 /*id*/, ObjectGuid /*target*/)
{
}

void GroupNotices::GroupList(Group const& /*group*/, Player& /*player*/, Group::MemberSlot const& /*slot*/, uint8 /*lfgState*/, uint32 /*lfgDungeon*/, uint32 /*sequence*/)
{
}

void GroupNotices::PartyMemberStatsChanged(Group& /*group*/, Player& /*member*/)
{
}

void GroupNotices::ReadyCheckOffline(Group& /*group*/, ObjectGuid /*member*/)
{
}

GroupNotices& GroupNotices::Installed()
{
    GroupNotices* notices = s_groupNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseGroupNotices();
}

GroupNotices* GroupNotices::Install(GroupNotices* notices)
{
    return s_groupNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
