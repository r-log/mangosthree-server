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

/// The arena teams' repository on the global CharacterDatabase backed by fakes: the two load rows
/// read into their values, and every statement the team writes, with the three transactions
/// (create, disband, save) each queued as one item. What reaches the asynchronous connection
/// after ExecuteQueuedForTest() is what the server would send to MySQL, a bound name substituted.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/social/ArenaTeamRepository.h"

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

TEST(ArenaTeamRepository_ReadsTheTeamAndTheMemberRows)
{
    std::vector<ArenaTeamRow> teams;
    {
        Db db;
        db.query.Answer("SELECT", FakeRows{FakeRow{"5", "Aldors", "7777", "2", "11", "12", "13", "14", "15",
                                                   "1500", "21", "22", "23", "24", "25"}});
        teams = ArenaTeamRepository::QueryTeams();
    }
    REQUIRE(teams.size() == 1);
    ArenaTeamRow const& team = teams[0];
    CHECK_EQ(team.id, 5u);
    CHECK_STR(team.name, "Aldors");
    CHECK_EQ(team.captainGuid, 7777u);
    CHECK_EQ(team.type, 2u);
    CHECK_EQ(team.backgroundColor, 11u);
    CHECK_EQ(team.emblemStyle, 12u);
    CHECK_EQ(team.emblemColor, 13u);
    CHECK_EQ(team.borderStyle, 14u);
    CHECK_EQ(team.borderColor, 15u);
    CHECK_EQ(team.stats.rating, 1500u);
    CHECK_EQ(team.stats.gamesWeek, 21u);
    CHECK_EQ(team.stats.winsWeek, 22u);
    CHECK_EQ(team.stats.gamesSeason, 23u);
    CHECK_EQ(team.stats.winsSeason, 24u);
    CHECK_EQ(team.stats.rank, 25u);

    std::vector<ArenaTeamMemberRow> members;
    {
        Db db;
        db.query.Answer("SELECT", FakeRows{FakeRow{"5", "42", "3", "2", "30", "20", "1600", "Kael", "8"}});
        members = ArenaTeamRepository::QueryTeamMembers();
    }
    REQUIRE(members.size() == 1);
    ArenaTeamMemberRow const& member = members[0];
    CHECK_EQ(member.teamId, 5u);
    CHECK_EQ(member.stats.guid, 42u);
    CHECK_EQ(member.stats.gamesWeek, 3u);
    CHECK_EQ(member.stats.winsWeek, 2u);
    CHECK_EQ(member.stats.gamesSeason, 30u);
    CHECK_EQ(member.stats.winsSeason, 20u);
    CHECK_EQ(member.stats.personalRating, 1600u);
    CHECK_STR(member.name, "Kael");
    CHECK_EQ(uint32(member.playerClass), 8u);
}

// The create is one queued transaction: the leftover members' DELETE, the team with its name
// bound, then the stats row.
TEST(ArenaTeamRepository_CreateIsOneTransaction)
{
    Db db;
    ArenaTeamRow team;
    team.id = 5;
    team.name = "Aldors";
    team.captainGuid = 7777;
    team.type = 2;
    team.backgroundColor = 11;
    team.stats.rating = 1500;
    {
        TickGuard::Scope scope;
        ArenaTeamRepository::Create(team);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `arena_team_member` WHERE `arenateamid`='5'",
        "INSERT INTO `arena_team` (`arenateamid`,`name`,`captainguid`,`type`,`BackgroundColor`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`) "
        "VALUES('5','Aldors','7777','2','11','0','0','0','0')",
        "INSERT INTO `arena_team_stats` (`arenateamid`, `rating`, `games_week`, `wins_week`, `games_season`, `wins_season`, `rank`) VALUES "
        "('5', '1500', '0', '0', '0', '0', '0')",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The single writes, each its own statement, in the argument order its text names them.
TEST(ArenaTeamRepository_SingleWrites)
{
    Db db;
    ArenaTeamRepository::InsertMember(5, 42, 1500);
    ArenaTeamRepository::DeleteMembersOfTeam(4);
    ArenaTeamRepository::SetCaptain(5, 43);
    ArenaTeamRepository::DeleteMember(5, 42);
    ArenaTeamRepository::SetEmblem(5, 1, 2, 3, 4, 6);
    ArenaTeamRepository::SetRating(5, 1510);
    ArenaTeamRepository::SetGamesWeek(5, 7);
    ArenaTeamRepository::SetWinsWeek(5, 6);
    ArenaTeamRepository::SetGamesSeason(5, 70);
    ArenaTeamRepository::SetWinsSeason(5, 60);
    ArenaTeamRepository::SetRank(5, 9);
    CheckLines(db.Take(), {
        "INSERT INTO `arena_team_member` (`arenateamid`, `guid`, `personal_rating`) VALUES ('5', '42', '1500')",
        "DELETE FROM `arena_team_member` WHERE `arenateamid` = '4'",
        "UPDATE `arena_team` SET `captainguid` = '43' WHERE `arenateamid` = '5'",
        "DELETE FROM `arena_team_member` WHERE `arenateamid` = '5' AND `guid` = '42'",
        "UPDATE `arena_team` SET `BackgroundColor`='1', `EmblemStyle`='2', `EmblemColor`='3', `BorderStyle`='4', `BorderColor`='6' WHERE `arenateamid`='5'",
        "UPDATE `arena_team_stats` SET `rating` = '1510' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_stats` SET `games_week` = '7' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_stats` SET `wins_week` = '6' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_stats` SET `games_season` = '70' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_stats` SET `wins_season` = '60' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_stats` SET `rank` = '9' WHERE `arenateamid` = '5'",
    }, __LINE__);
}

// The disband and the save are each one queued transaction: the team, its members and its stats
// deleted; the stats and then each member's written, in the order handed.
TEST(ArenaTeamRepository_DisbandAndSaveAreTransactions)
{
    Db db;
    {
        TickGuard::Scope scope;
        ArenaTeamRepository::Disband(5);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `arena_team` WHERE `arenateamid` = '5'",
        "DELETE FROM `arena_team_member` WHERE `arenateamid` = '5'",
        "DELETE FROM `arena_team_stats` WHERE `arenateamid` = '5'",
    }, __LINE__);

    ArenaTeamStatsRow stats;
    stats.rating = 1520;
    stats.gamesWeek = 8;
    stats.winsWeek = 5;
    stats.gamesSeason = 80;
    stats.winsSeason = 50;
    stats.rank = 3;
    ArenaTeamMemberStatsRow first;
    first.guid = 42;
    first.gamesWeek = 8;
    first.winsWeek = 5;
    first.gamesSeason = 80;
    first.winsSeason = 50;
    first.personalRating = 1650;
    ArenaTeamMemberStatsRow second;
    second.guid = 43;
    second.personalRating = 1500;
    {
        TickGuard::Scope scope;
        ArenaTeamRepository::SaveStats(5, stats, { first, second });
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "UPDATE `arena_team_stats` SET `rating` = '1520',`games_week` = '8',`games_season` = '80',`rank` = '3',`wins_week` = '5',`wins_season` = '50' WHERE `arenateamid` = '5'",
        "UPDATE `arena_team_member` SET `played_week` = '8', `wons_week` = '5', `played_season` = '80', `wons_season` = '50', `personal_rating` = '1650' WHERE `arenateamid` = '5' AND `guid` = '42'",
        "UPDATE `arena_team_member` SET `played_week` = '0', `wons_week` = '0', `played_season` = '0', `wons_season` = '0', `personal_rating` = '1500' WHERE `arenateamid` = '5' AND `guid` = '43'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// Each column of a arena team member (`arena_team_member`), with its team's type lands in its
// member, and the rows come back in order.
TEST(ArenaTeamRepository_MemberTeamTypes)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<ArenaTeamMemberTypeRow> rows = ArenaTeamRepository::QueryMemberTeamTypes();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].arenateamid, 1001u);
    CHECK_EQ(rows[0].type, 1002u);
    CHECK_EQ(rows[1].guid, 2000u);
}
