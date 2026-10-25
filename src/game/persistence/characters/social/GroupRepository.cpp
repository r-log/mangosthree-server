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

#include "GroupRepository.h"
#include "Database/DatabaseEnv.h"

namespace GroupRepository
{
    void Store(GroupRow const& row)
    {
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` ='%u'", row.id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` ='%u'", row.id);

        CharacterDatabase.PExecute("INSERT INTO `groups` (`groupId`,`leaderGuid`,`mainTank`,`mainAssistant`,`lootMethod`,`looterGuid`,`lootThreshold`,`icon1`,`icon2`,`icon3`,`icon4`,`icon5`,`icon6`,`icon7`,`icon8`,`groupType`,`difficulty`,`raiddifficulty`) "
                                   "VALUES ('%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u','%u','%u')",
                                   row.id, row.leaderGuid, row.mainTank, row.mainAssistant, row.lootMethod,
                                   row.looterGuid, row.lootThreshold,
                                   row.targetIcons[0], row.targetIcons[1],
                                   row.targetIcons[2], row.targetIcons[3],
                                   row.targetIcons[4], row.targetIcons[5],
                                   row.targetIcons[6], row.targetIcons[7],
                                   row.groupType, row.difficulty, row.raidDifficulty);
    }

    void Disband(uint32 groupId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId`='%u'", groupId);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId`='%u'", groupId);
        CharacterDatabase.CommitTransaction();
    }

    void SetGroupType(uint32 groupId, uint32 groupType)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `groupType` = %u WHERE `groupId`='%u'", groupType, groupId);
    }

    void SetLeader(uint32 groupId, uint32 leaderGuid)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `leaderGuid`='%u' WHERE `groupId`='%u'", leaderGuid, groupId);
    }

    void SetMainTank(uint32 groupId, uint32 memberGuid)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainTank`='%u' WHERE `groupId`='%u'", memberGuid, groupId);
    }

    void SetMainAssistant(uint32 groupId, uint32 memberGuid)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainAssistant`='%u' WHERE `groupId`='%u'",
                                   memberGuid, groupId);
    }

    void SetDungeonDifficulty(uint32 groupId, uint32 difficulty)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `difficulty` = %u WHERE `groupId`='%u'", difficulty, groupId);
    }

    void SetRaidDifficulty(uint32 groupId, uint32 difficulty)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `raiddifficulty` = %u WHERE `groupId` = '%u'", difficulty, groupId);
    }

    void ConvertToParty(uint32 groupId, uint32 groupType)
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `groupType` = %u, `mainTank` = '0', `mainAssistant` = '0' WHERE `groupId`='%u'",
                                   groupType, groupId);
        CharacterDatabase.PExecute("UPDATE `group_member` SET `subgroup` = '0', `assistant` = '0' WHERE `groupId`='%u'", groupId);
    }

    void InsertMember(uint32 groupId, uint32 memberGuid, uint32 assistant, uint32 subgroup, uint32 roles)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_member` (`groupId`,`memberGuid`,`assistant`,`subgroup`,`roles`) VALUES ('%u','%u','%u','%u','%u')",
                                   groupId, memberGuid, assistant, subgroup, roles);
    }

    void DeleteMember(uint32 memberGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid`='%u'", memberGuid);
    }

    void SetMemberSubgroup(uint32 memberGuid, uint32 subgroup)
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `subgroup`='%u' WHERE `memberGuid`='%u'", subgroup, memberGuid);
    }

    void SetMemberAssistant(uint32 memberGuid, uint32 assistant)
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `assistant`='%u' WHERE `memberGuid`='%u'", assistant, memberGuid);
    }

    void SetMemberRoles(uint32 memberGuid, uint32 roles)
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `roles` = '%u' WHERE `memberGuid`='%u'",
                                   roles, memberGuid);
    }

    void DeleteBindsReplacedByLeader(uint32 leaderGuid, uint32 newLeaderGuid)
    {
        CharacterDatabase.PExecute(
            "DELETE FROM `group_instance` WHERE `leaderguid`='%u' AND (`permanent` = 1 OR "
            "`instance` IN (SELECT `instance` FROM `character_instance` WHERE `guid` = '%u')"
            ")", leaderGuid, newLeaderGuid);
    }

    void MoveBindsToLeader(uint32 oldLeaderGuid, uint32 newLeaderGuid)
    {
        CharacterDatabase.PExecute("UPDATE `group_instance` SET `leaderGuid`='%u' WHERE `leaderGuid` = '%u'",
                                   newLeaderGuid, oldLeaderGuid);
    }

    void DeleteBindsOfInstance(uint32 instanceId)
    {
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", instanceId);
    }

    void UpdateBind(uint32 leaderGuid, uint32 oldInstanceId, uint32 instanceId, uint32 permanent)
    {
        CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                                   instanceId, permanent, leaderGuid, oldInstanceId);
    }

    void InsertBind(uint32 leaderGuid, uint32 instanceId, uint32 permanent)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_instance` (`leaderGuid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
                                   leaderGuid, instanceId, permanent);
    }

    void DeleteBind(uint32 leaderGuid, uint32 instanceId)
    {
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                                   leaderGuid, instanceId);
    }

    QueryResult* QueryGroupIds()
    {
        return CharacterDatabase.Query("SELECT `groupId` FROM `groups`");
    }

    void DeleteInvalidGroup(uint32 groupId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` = '%u'", groupId);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` = '%u'", groupId);
        CharacterDatabase.CommitTransaction();
    }

    void RenumberGroup(uint32 newId, uint32 oldId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("UPDATE `groups` SET `groupId` = '%u' WHERE `groupId` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `group_member` SET `groupId` = '%u' WHERE `groupId` = '%u'", newId, oldId);
        CharacterDatabase.CommitTransaction();
    }

    QueryResult* QueryHighestId()
    {
        return CharacterDatabase.Query("SELECT MAX(`groupId`) FROM `groups`");
    }

    QueryResult* QueryGroups()
    {
        //                                      0           1                2             3             4                5        6        7        8        9        10       11       12       13           14            15                16            17
        return CharacterDatabase.Query("SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `groupType`, `difficulty`, `raiddifficulty`, `leaderGuid`, `groupId` FROM `groups`");
    }

    QueryResult* QueryMembers()
    {
        //                                     0             1            2           3          4
        return CharacterDatabase.Query("SELECT `memberGuid`, `assistant`, `subgroup`, `groupId`, `roles` FROM `group_member` ORDER BY `groupId`");
    }

    void DeleteInvalidMember(uint32 memberGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid` = '%u'", memberGuid);
    }

    QueryResult* QueryInstanceBinds()
    {
        //                                                       0             1      2           3                       4             5
        return CharacterDatabase.Query("SELECT `group_instance`.`leaderGuid`, `map`, `instance`, `permanent`, `instance`.`difficulty`, `resettime`, "
                                       // 6
                                       "(SELECT COUNT(*) FROM `character_instance` WHERE `guid` = `group_instance`.`leaderGuid` AND `instance` = `group_instance`.`instance` AND `permanent` = 1 LIMIT 1), "
                                       // 7                              8
                                       " `groups`.`groupId`, `instance`.`encountersMask` "
                                       "FROM `group_instance` LEFT JOIN `instance` ON `instance` = `id` LEFT JOIN `groups` ON `groups`.`leaderGUID` = `group_instance`.`leaderGUID` ORDER BY `leaderGuid`");
    }

    void RepairLeaders()
    {
        CharacterDatabase.Execute(
            "UPDATE `groups` SET `leaderGuid` = (SELECT `memberGuid` FROM `group_member` "
            "WHERE `group_member`.`groupId` = `groups`.`groupId` ORDER BY `assistant` DESC LIMIT 1) "
            "WHERE `leaderGuid` NOT IN (SELECT `memberGuid` FROM `group_member` "
            "WHERE `group_member`.`groupId` = `groups`.`groupId`)");
    }
}
