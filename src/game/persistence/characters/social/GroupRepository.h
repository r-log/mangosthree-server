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

#ifndef MANGOS_H_GROUPREPOSITORY
#define MANGOS_H_GROUPREPOSITORY

#include "Platform/Define.h"

#include <optional>

class QueryResult;

/// The number of target icons a saved group row holds.
enum
{
    GROUP_ROW_TARGET_ICONS = 8
};

/// A group's saved row, in the order of the INSERT's columns.
struct GroupRow
{
    uint32 id = 0;
    uint32 leaderGuid = 0;
    uint32 mainTank = 0;
    uint32 mainAssistant = 0;
    uint32 lootMethod = 0;
    uint32 looterGuid = 0;
    uint32 lootThreshold = 0;
    uint64 targetIcons[GROUP_ROW_TARGET_ICONS] = {};
    uint32 groupType = 0;
    uint32 difficulty = 0;
    uint32 raidDifficulty = 0;
};

/// The groups' saved rows (`groups`, `group_member`, `group_instance`). The start-up reads run
/// synchronously; every write is queued on the character database, and a write called inside
/// CharacterTransaction joins that transaction.
namespace GroupRepository
{
    /// Deletes any group and member rows left under the group's id, then inserts its row.
    void Store(GroupRow const& row);

    /// Deletes the group's row and its members' rows in one transaction.
    void Disband(uint32 groupId);

    /// Each sets one column of the group's row.
    void SetGroupType(uint32 groupId, uint32 groupType);
    void SetLeader(uint32 groupId, uint32 leaderGuid);
    void SetMainTank(uint32 groupId, uint32 memberGuid);
    void SetMainAssistant(uint32 groupId, uint32 memberGuid);
    void SetDungeonDifficulty(uint32 groupId, uint32 difficulty);
    void SetRaidDifficulty(uint32 groupId, uint32 difficulty);

    /// Sets the group's type and clears its main tank and main assistant, then puts every member in
    /// subgroup 0 without assistant rights: two statements.
    void ConvertToParty(uint32 groupId, uint32 groupType);

    /// Inserts a member's row.
    void InsertMember(uint32 groupId, uint32 memberGuid, uint32 assistant, uint32 subgroup, uint32 roles);

    /// Deletes a member's row.
    void DeleteMember(uint32 memberGuid);

    /// Each sets one column of a member's row.
    void SetMemberSubgroup(uint32 memberGuid, uint32 subgroup);
    void SetMemberAssistant(uint32 memberGuid, uint32 assistant);
    void SetMemberRoles(uint32 memberGuid, uint32 roles);

    /// Deletes the leader's permanent binds and its binds of every instance the new leader is bound to.
    void DeleteBindsReplacedByLeader(uint32 leaderGuid, uint32 newLeaderGuid);

    /// Moves every bind of the old leader to the new one.
    void MoveBindsToLeader(uint32 oldLeaderGuid, uint32 newLeaderGuid);

    /// Deletes every group's bind of an instance.
    void DeleteBindsOfInstance(uint32 instanceId);

    /// Points the leader's bind of oldInstanceId at instanceId with the given permanence.
    void UpdateBind(uint32 leaderGuid, uint32 oldInstanceId, uint32 instanceId, uint32 permanent);

    /// Inserts a bind of the leader.
    void InsertBind(uint32 leaderGuid, uint32 instanceId, uint32 permanent);

    /// Deletes the leader's bind of an instance.
    void DeleteBind(uint32 leaderGuid, uint32 instanceId);

    /// Selects every group's id, as the start-up renumbering reads them, synchronously.
    QueryResult* QueryGroupIds();

    /// Deletes a group row the start-up renumbering cannot use and its members, in one transaction.
    void DeleteInvalidGroup(uint32 groupId);

    /// Renumbers a group's row and its members, in one transaction.
    void RenumberGroup(uint32 newId, uint32 oldId);

    /// Selects the highest group id, at start-up.
    std::optional<uint32> QueryHighestId();

    /// Selects every group's row, at start-up.
    QueryResult* QueryGroups();

    /// Selects every group member, ordered by group id, at start-up.
    QueryResult* QueryMembers();

    /// Deletes a member row the start-up load cannot use.
    void DeleteInvalidMember(uint32 memberGuid);

    /// Selects every group instance bind with its instance and group, ordered by leader, at start-up.
    QueryResult* QueryInstanceBinds();

    /// Gives every group whose leader left it a member as its leader, assistants first, at start-up
    /// and shutdown.
    void RepairLeaders();
}

#endif
