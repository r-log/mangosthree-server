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

// Each column of a `groups` row lands in its member, and the rows come back in order.
TEST(GroupRepository_GroupIds)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"},
        FakeRow{"2000"},
    });
    std::vector<GroupIdRow> rows = GroupRepository::QueryGroupIds();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].groupId, 1000u);
    CHECK_EQ(rows[1].groupId, 2000u);
}

// Each column of a `groups` row as the group load reads it lands in its member, and the rows come
// back in order.
TEST(GroupRepository_Groups)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "102", "1003", "10004", "4294967301", "4294967302", "4294967303",
            "4294967304", "4294967305", "4294967306", "4294967307", "4294967308", "113", "114",
            "115", "1016", "1017"},
        FakeRow{"2000", "2001", "202", "2003", "20004", "8589934597", "8589934598", "8589934599",
            "8589934600", "8589934601", "8589934602", "8589934603", "8589934604", "213", "214",
            "215", "2016", "2017"},
    });
    std::vector<GroupDataRow> rows = GroupRepository::QueryGroups();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].mainTank, 1000u);
    CHECK_EQ(rows[0].mainAssistant, 1001u);
    CHECK_EQ(rows[0].lootMethod, uint8(102));
    CHECK_EQ(rows[0].looterGuid, 1003u);
    CHECK_EQ(rows[0].lootThreshold, uint16(10004));
    CHECK_EQ(rows[0].icons[0], uint64(4294967301ull));
    CHECK_EQ(rows[0].icons[1], uint64(4294967302ull));
    CHECK_EQ(rows[0].icons[2], uint64(4294967303ull));
    CHECK_EQ(rows[0].icons[3], uint64(4294967304ull));
    CHECK_EQ(rows[0].icons[4], uint64(4294967305ull));
    CHECK_EQ(rows[0].icons[5], uint64(4294967306ull));
    CHECK_EQ(rows[0].icons[6], uint64(4294967307ull));
    CHECK_EQ(rows[0].icons[7], uint64(4294967308ull));
    CHECK_EQ(rows[0].groupType, uint8(113));
    CHECK_EQ(rows[0].difficulty, uint8(114));
    CHECK_EQ(rows[0].raidDifficulty, uint8(115));
    CHECK_EQ(rows[0].leaderGuid, 1016u);
    CHECK_EQ(rows[0].groupId, 1017u);
    CHECK_EQ(rows[1].mainTank, 2000u);
}

// Each column of a `group_member` row lands in its member, and the rows come back in order.
TEST(GroupRepository_Members)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1", "102", "1003", "104"},
        FakeRow{"2000", "1", "202", "2003", "204"},
    });
    std::vector<GroupMemberRow> rows = GroupRepository::QueryMembers();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].memberGuid, 1000u);
    CHECK_EQ(rows[0].assistant, true);
    CHECK_EQ(rows[0].subgroup, uint8(102));
    CHECK_EQ(rows[0].groupId, 1003u);
    CHECK_EQ(rows[0].roles, uint8(104));
    CHECK_EQ(rows[1].memberGuid, 2000u);
}

// Each column of a group instance bind (`group_instance`), with its instance and group, and how
// many permanent binds the leader holds to the instance lands in its member, and the rows come back
// in order.
TEST(GroupRepository_InstanceBinds)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1", "104", "4294967301", "1006", "1007", "1008"},
        FakeRow{"2000", "2001", "2002", "1", "204", "8589934597", "2006", "2007", "2008"},
    });
    std::vector<GroupInstanceBindRow> rows = GroupRepository::QueryInstanceBinds();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].leaderGuid, 1000u);
    CHECK_EQ(rows[0].map, 1001u);
    CHECK_EQ(rows[0].instance, 1002u);
    CHECK_EQ(rows[0].permanent, true);
    CHECK_EQ(rows[0].difficulty, uint8(104));
    CHECK_EQ(rows[0].resettime, uint64(4294967301ull));
    CHECK_EQ(rows[0].leaderPermanentBindCount, 1006u);
    CHECK_EQ(rows[0].groupId, 1007u);
    CHECK_EQ(rows[0].encountersMask, 1008u);
    CHECK_EQ(rows[1].leaderGuid, 2000u);
}
