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

/// The groups' repository on the global CharacterDatabase backed by fakes: every statement a group
/// writes, in order, with the create and the leader change each queued as one transaction around
/// the writes the group issues one at a time, and the disband as one of its own. What reaches the
/// asynchronous connection after ExecuteQueuedForTest() is what the server would send to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GroupRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The create: the group's row stored and its first member inserted between CharacterTransaction's
// Begin and Commit reach the database as one queued transaction.
TEST(GroupRepository_CreateJoinsOneTransaction)
{
    Db db;
    GroupRow row;
    row.id = 12;
    row.leaderGuid = 7;
    row.lootMethod = 3;
    row.looterGuid = 7;
    row.lootThreshold = 2;
    row.targetIcons[0] = 0xF130000000000005ULL;
    row.targetIcons[7] = 9;
    row.groupType = 0;
    row.difficulty = 1;
    row.raidDifficulty = 2;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        GroupRepository::Store(row);
        GroupRepository::InsertMember(12, 7, 0, 0, 0);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `groups` WHERE `groupId` ='12'",
        "DELETE FROM `group_member` WHERE `groupId` ='12'",
        "INSERT INTO `groups` (`groupId`,`leaderGuid`,`mainTank`,`mainAssistant`,`lootMethod`,`looterGuid`,`lootThreshold`,`icon1`,`icon2`,`icon3`,`icon4`,`icon5`,`icon6`,`icon7`,`icon8`,`groupType`,`difficulty`,`raiddifficulty`) "
        "VALUES ('12','7','0','0','3','7','2','17379390962022744069','0','0','0','0','0','0','9','0','1','2')",
        "INSERT INTO `group_member` (`groupId`,`memberGuid`,`assistant`,`subgroup`,`roles`) VALUES ('12','7','0','0','0')",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The leader change: the replaced binds deleted, the solo binds moved and the leader set, as one
// queued transaction.
TEST(GroupRepository_LeaderChangeJoinsOneTransaction)
{
    Db db;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        GroupRepository::DeleteBindsReplacedByLeader(7, 8);
        GroupRepository::MoveBindsToLeader(7, 8);
        GroupRepository::SetLeader(12, 8);
        CharacterTransaction::Commit();
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `group_instance` WHERE `leaderguid`='7' AND (`permanent` = 1 OR "
        "`instance` IN (SELECT `instance` FROM `character_instance` WHERE `guid` = '8'))",
        "UPDATE `group_instance` SET `leaderGuid`='8' WHERE `leaderGuid` = '7'",
        "UPDATE `groups` SET `leaderGuid`='8' WHERE `groupId`='12'",
    }, __LINE__);
}

// The disband is one queued transaction of its own.
TEST(GroupRepository_DisbandIsOneTransaction)
{
    Db db;
    {
        TickGuard::Scope scope;
        GroupRepository::Disband(12);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `groups` WHERE `groupId`='12'",
        "DELETE FROM `group_member` WHERE `groupId`='12'",
    }, __LINE__);
}

// The single writes, each its own statement, in the argument order its text names them.
TEST(GroupRepository_SingleWrites)
{
    Db db;
    GroupRepository::SetGroupType(12, 2);
    GroupRepository::SetMainTank(12, 8);
    GroupRepository::SetMainAssistant(12, 9);
    GroupRepository::SetDungeonDifficulty(12, 1);
    GroupRepository::SetRaidDifficulty(12, 3);
    GroupRepository::ConvertToParty(12, 0);
    GroupRepository::DeleteMember(9);
    GroupRepository::SetMemberSubgroup(8, 4);
    GroupRepository::SetMemberAssistant(8, 1);
    GroupRepository::SetMemberRoles(8, 6);
    GroupRepository::DeleteBindsOfInstance(300);
    GroupRepository::UpdateBind(8, 300, 301, 1);
    GroupRepository::InsertBind(8, 302, 0);
    GroupRepository::DeleteBind(8, 302);
    CheckLines(db.Take(), {
        "UPDATE `groups` SET `groupType` = 2 WHERE `groupId`='12'",
        "UPDATE `groups` SET `mainTank`='8' WHERE `groupId`='12'",
        "UPDATE `groups` SET `mainAssistant`='9' WHERE `groupId`='12'",
        "UPDATE `groups` SET `difficulty` = 1 WHERE `groupId`='12'",
        "UPDATE `groups` SET `raiddifficulty` = 3 WHERE `groupId` = '12'",
        "UPDATE `groups` SET `groupType` = 0, `mainTank` = '0', `mainAssistant` = '0' WHERE `groupId`='12'",
        "UPDATE `group_member` SET `subgroup` = '0', `assistant` = '0' WHERE `groupId`='12'",
        "DELETE FROM `group_member` WHERE `memberGuid`='9'",
        "UPDATE `group_member` SET `subgroup`='4' WHERE `memberGuid`='8'",
        "UPDATE `group_member` SET `assistant`='1' WHERE `memberGuid`='8'",
        "UPDATE `group_member` SET `roles` = '6' WHERE `memberGuid`='8'",
        "DELETE FROM `group_instance` WHERE `instance` = '300'",
        "UPDATE `group_instance` SET `instance` = '301', `permanent` = '1' WHERE `leaderGuid` = '8' AND `instance` = '300'",
        "INSERT INTO `group_instance` (`leaderGuid`, `instance`, `permanent`) VALUES ('8', '302', '0')",
        "DELETE FROM `group_instance` WHERE `leaderGuid` = '8' AND `instance` = '302'",
    }, __LINE__);
}
