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

/// The guilds' repository on the global CharacterDatabase backed by fakes: every queued write in
/// order with its values substituted as the server would send them to MySQL after
/// ExecuteQueuedForTest(); the create and the rank delete are one queued transaction each, and a
/// disband's writes join the transaction the guild opens around them, as does a rollback drop one.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GuildRepository.h"

#include <cstdio>
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

// The create is one queued transaction: the leftover members' DELETE, then the guild's row with its
// strings bound.
TEST(GuildRepository_CreateIsOneTransaction)
{
    Db db;
    GuildRow row;
    row.id = 3;
    row.name = "Wardens";
    row.leaderGuid = 7;
    row.motd = "No message set.";
    row.createDate = 1700000000;
    {
        TickGuard::Scope scope;
        GuildRepository::Create(row);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `guild_member` WHERE `guildid`='3'",
        "INSERT INTO `guild` (`guildid`,`name`,`leaderguid`,`info`,`motd`,`createdate`,`EmblemStyle`,`EmblemColor`,`BorderStyle`,`BorderColor`,`BackgroundColor`,`BankMoney`) "
        "VALUES('3','Wardens','7','','No message set.','1700000000','0','0','0','0','0','0')",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The guild, member and rank writes, each its own statement, in the argument order their text names
// them.
TEST(GuildRepository_SingleWrites)
{
    Db db;
    GuildRepository::SetMotd(3, "Raid at 8");
    GuildRepository::SetInfo(3, "Be nice");
    GuildRepository::SetLeader(3, 8);
    GuildRepository::SetEmblem(3, 1, 2, 3, 4, 5);
    GuildRepository::InsertMember(3, 8, 4, "", "");
    GuildRepository::SetMemberRank(8, 1);
    GuildRepository::SetMemberPublicNote(8, "Healer");
    GuildRepository::SetMemberOfficerNote(8, "Alt of Bob");
    GuildRepository::DeleteMember(8);
    GuildRepository::DeleteOrphanMembers(2);
    GuildRepository::DeleteRanks(3);
    GuildRepository::DeleteOrphanRanks(2);
    GuildRepository::DeleteBankRights(3);
    GuildRepository::InsertRank(3, 0, "Guild Master", 0xFFFFF);
    GuildRepository::InsertBankRight(3, 0, 5);
    GuildRepository::SetRankName(3, 5, "Recruit");
    GuildRepository::SetRankRights(3, 5, 67);
    GuildRepository::DeleteEventLogEntry(3, 9);
    GuildRepository::InsertEventLogEntry(3, 9, 2, 7, 8, 0, 1700000100);
    GuildRepository::TrimEventLog(100);
    GuildRepository::TrimBankEventLog(25);
    CheckLines(db.Take(), {
        "UPDATE `guild` SET `motd` = 'Raid at 8' WHERE `guildid` = '3'",
        "UPDATE `guild` SET `info` = 'Be nice' WHERE `guildid` = '3'",
        "UPDATE `guild` SET `leaderguid`='8' WHERE `guildid`='3'",
        "UPDATE `guild` SET `EmblemStyle`=1, `EmblemColor`=2, `BorderStyle`=3, `BorderColor`=4, `BackgroundColor`=5 WHERE `guildid` = 3",
        "INSERT INTO `guild_member` (`guildid`,`guid`,`rank`,`pnote`,`offnote`) VALUES ('3','8','4','','')",
        "UPDATE `guild_member` SET `rank`='1' WHERE `guid`='8'",
        "UPDATE `guild_member` SET `pnote` = 'Healer' WHERE `guid` = '8'",
        "UPDATE `guild_member` SET `offnote` = 'Alt of Bob' WHERE `guid` = '8'",
        "DELETE FROM `guild_member` WHERE `guid` = '8'",
        "DELETE FROM `guild_member` WHERE `guildid` = '2'",
        "DELETE FROM `guild_rank` WHERE `guildid`='3'",
        "DELETE FROM `guild_rank` WHERE `guildid` = '2'",
        "DELETE FROM `guild_bank_right` WHERE `guildid` = '3'",
        "INSERT INTO `guild_rank` (`guildid`,`rid`,`rname`,`rights`) VALUES ('3','0','Guild Master','1048575')",
        "INSERT INTO `guild_bank_right` (`guildid`,`TabId`,`rid`) VALUES ('3','0','5')",
        "UPDATE `guild_rank` SET `rname` = 'Recruit' WHERE `rid` = '5' AND `guildid` = '3'",
        "UPDATE `guild_rank` SET `rights`='67' WHERE `rid`='5' AND `guildid`='3'",
        "DELETE FROM `guild_eventlog` WHERE `guildid`='3' AND `LogGuid`='9'",
        "INSERT INTO `guild_eventlog` (`guildid`, `LogGuid`, `EventType`, `PlayerGuid1`, `PlayerGuid2`, `NewRank`, `TimeStamp`) VALUES ('3','9','2','7','8','0','1700000100')",
        "DELETE FROM `guild_eventlog` WHERE `LogGuid` > '100'",
        "DELETE FROM `guild_bank_eventlog` WHERE `LogGuid` > '25'",
    }, __LINE__);
}

// A rank switch binds the rank's name, as the rename does: the connection escapes both names when the
// queued writes go out, and neither escapes on the calling thread.
TEST(GuildRepository_RankWritesBindTheName)
{
    Db db;
    {
        TickGuard::Scope scope;
        GuildRepository::ReplaceRank(3, 2, "Ann's", 67, 0);
        GuildRepository::SetRankName(3, 4, "Bob's");
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CHECK_EQ(db.query.escaped.size(), size_t(0));
    CHECK_EQ(db.Take().size(), size_t(2));
    CheckLines(db.query.escaped, { "Ann's", "Bob's" }, __LINE__);
}

// The rank delete is one queued transaction; a rank switch's two REPLACEs and a disband's two groups
// of deletes each join the transaction the guild opens around them.
TEST(GuildRepository_Transactions)
{
    Db db;
    {
        TickGuard::Scope scope;
        GuildRepository::DeleteRank(3, 6);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `guild_rank` WHERE `rid` ='6' AND `guildid` ='3'",
        "DELETE FROM `guild_bank_right` WHERE `rid` ='6' AND `guildid` ='3'",
        "UPDATE `guild_rank` SET `rid` = `rid` - 1 WHERE `rid` > '6' AND `guildid` ='3'",
        "UPDATE `guild_bank_right` SET `rid` = `rid` - 1 WHERE `rid` > '6' AND `guildid` ='3'",
    }, __LINE__);

    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        GuildRepository::ReplaceBankRight(3, 0, 2, 3, 10);
        GuildRepository::ReplaceRank(3, 2, "Veteran", 67, 0);
        CharacterTransaction::Commit();
        CharacterTransaction::Begin();
        GuildRepository::DeleteGuildRanksAndTabs(3);
        GuildRepository::DeleteBankAndLogs(3);
        CharacterTransaction::Commit();
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    CheckLines(db.Take(), {
        "REPLACE INTO guild_bank_right (guildid,TabId,rid,gbright,SlotPerDay) VALUES ('3','0','2','3','10')",
        "REPLACE INTO guild_rank (guildid,rid,rname,rights,BankMoneyPerDay) VALUES ('3', '2', 'Veteran', '67', '0')",
        "DELETE FROM `guild` WHERE `guildid` = '3'",
        "DELETE FROM `guild_rank` WHERE `guildid` = '3'",
        "DELETE FROM `guild_bank_tab` WHERE `guildid` = '3'",
        "DELETE FROM `guild_bank_item` WHERE `guildid` = '3'",
        "DELETE FROM `guild_bank_right` WHERE `guildid` = '3'",
        "DELETE FROM `guild_bank_eventlog` WHERE `guildid` = '3'",
        "DELETE FROM `guild_eventlog` WHERE `guildid` = '3'",
    }, __LINE__);

    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        GuildRepository::SetLeader(3, 9);
        CharacterTransaction::Rollback();
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
    }
    CheckLines(db.Take(), {}, __LINE__);
}

// Each column of a `guild_eventlog` row lands in its member, and the rows come back in order.
TEST(GuildRepository_EventLog)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "104", "4294967301"},
        FakeRow{"2000", "201", "2002", "2003", "204", "8589934597"},
    });
    std::vector<GuildEventLogRow> rows = GuildRepository::QueryEventLog(1, 1);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].logGuid, 1000u);
    CHECK_EQ(rows[0].eventType, uint8(101));
    CHECK_EQ(rows[0].playerGuid1, 1002u);
    CHECK_EQ(rows[0].playerGuid2, 1003u);
    CHECK_EQ(rows[0].newRank, uint8(104));
    CHECK_EQ(rows[0].timeStamp, uint64(4294967301ull));
    CHECK_EQ(rows[1].logGuid, 2000u);
}

// Each column of a `guild_member` row lands in its member, and the rows come back in order.
TEST(GuildRepository_MemberRanks)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<GuildMemberRankRow> rows = GuildRepository::QueryMemberRanks();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].guildid, 1001u);
    CHECK_EQ(rows[0].rank, 1002u);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a guild as the startup load reads it lands in its member, and the rows come back
// in order.
TEST(GuildRepository_Guilds)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002", "1003", "1004", "1005", "1006", "1007", "s1.8", "s1.9",
            "4294967306", "4294967307", "1012"},
        FakeRow{"2000", "s2.1", "2002", "2003", "2004", "2005", "2006", "2007", "s2.8", "s2.9",
            "8589934602", "8589934603", "2012"},
    });
    std::vector<GuildDataRow> rows = GuildRepository::QueryGuilds();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guildId, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].leaderGuid, 1002u);
    CHECK_EQ(rows[0].emblemStyle, 1003u);
    CHECK_EQ(rows[0].emblemColor, 1004u);
    CHECK_EQ(rows[0].borderStyle, 1005u);
    CHECK_EQ(rows[0].borderColor, 1006u);
    CHECK_EQ(rows[0].backgroundColor, 1007u);
    CHECK_STR(rows[0].info, "s1.8");
    CHECK_STR(rows[0].motd, "s1.9");
    CHECK_EQ(rows[0].createDate, uint64(4294967306ull));
    CHECK_EQ(rows[0].bankMoney, uint64(4294967307ull));
    CHECK_EQ(rows[0].purchasedTabs, 1012u);
    CHECK_EQ(rows[1].guildId, 2000u);
}

// Each column of a `guild_rank` row lands in its member, and the rows come back in order.
TEST(GuildRepository_Ranks)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "s1.2", "1003", "1004"},
        FakeRow{"2000", "2001", "s2.2", "2003", "2004"},
    });
    std::vector<GuildRankRow> rows = GuildRepository::QueryRanks();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guildId, 1000u);
    CHECK_EQ(rows[0].rankId, 1001u);
    CHECK_STR(rows[0].name, "s1.2");
    CHECK_EQ(rows[0].rights, 1003u);
    CHECK_EQ(rows[0].bankMoneyPerDay, 1004u);
    CHECK_EQ(rows[1].guildId, 2000u);
}

// Each column of a guild member as the startup load reads it lands in its member, all eight bank tabs
// with their own columns, and the rows come back in order.
TEST(GuildRepository_Members)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "s1.3", "s1.4", "1005", "1006", "1007", "1008", "1009",
            "1010", "1011", "1012", "1013", "1014", "1015", "1016", "1017", "1018", "1019",
            "1020", "1021", "1022", "s1.23", "24", "25", "1026", "4294967323", "-1028"},
        FakeRow{"2000", "2001", "2002", "s2.3", "s2.4", "2005", "2006", "2007", "2008", "2009",
            "2010", "2011", "2012", "2013", "2014", "2015", "2016", "2017", "2018", "2019",
            "2020", "2021", "2022", "s2.23", "34", "35", "2026", "8589934619", "-2028"},
    });
    std::vector<GuildMemberDataRow> rows = GuildRepository::QueryMembers();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guildId, 1000u);
    CHECK_EQ(rows[0].guid, 1001u);
    CHECK_EQ(rows[0].rank, 1002u);
    CHECK_STR(rows[0].pnote, "s1.3");
    CHECK_STR(rows[0].offnote, "s1.4");
    CHECK_EQ(rows[0].bankResetTimeMoney, 1005u);
    CHECK_EQ(rows[0].bankRemMoney, 1006u);
    for (int i = 0; i < GUILD_MEMBER_DATA_ROW_TABS; ++i)
    {
        CHECK_EQ(rows[0].bankResetTimeTab[i], uint32(1007 + 2 * i));
        CHECK_EQ(rows[0].bankRemSlotsTab[i], uint32(1008 + 2 * i));
    }
    CHECK_STR(rows[0].name, "s1.23");
    CHECK_EQ(rows[0].level, uint8(24));
    CHECK_EQ(rows[0].classId, uint8(25));
    CHECK_EQ(rows[0].zone, 1026u);
    CHECK_EQ(rows[0].logoutTime, uint64(4294967323ull));
    CHECK_EQ(rows[0].account, -1028);
    CHECK_EQ(rows[1].guildId, 2000u);
    CHECK_STR(rows[1].name, "s2.23");
}

// The member query names the reset time and remaining slots of every bank tab, the last two included.
TEST(GuildRepository_MembersNameAllEightBankTabs)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{});
    GuildRepository::QueryMembers();
    REQUIRE(!db.query.executed.empty());
    for (int i = 0; i < GUILD_MEMBER_DATA_ROW_TABS; ++i)
    {
        char resetTime[32];
        char remSlots[32];
        std::snprintf(resetTime, sizeof(resetTime), "`BankResetTimeTab%d`", i);
        std::snprintf(remSlots, sizeof(remSlots), "`BankRemSlotsTab%d`", i);
        CHECK(db.query.executed.back().find(resetTime) != std::string::npos);
        CHECK(db.query.executed.back().find(remSlots) != std::string::npos);
    }
}

// Each column of a `guild_bank_right` row lands in its member, and the rows come back in order.
TEST(GuildRepository_BankRights)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "10003", "10004"},
        FakeRow{"2000", "201", "2002", "20003", "20004"},
    });
    std::vector<GuildBankRightRow> rows = GuildRepository::QueryBankRights();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guildId, 1000u);
    CHECK_EQ(rows[0].tabId, uint8(101));
    CHECK_EQ(rows[0].rankId, 1002u);
    CHECK_EQ(rows[0].rights, uint16(10003));
    CHECK_EQ(rows[0].slotPerDay, uint16(10004));
    CHECK_EQ(rows[1].guildId, 2000u);
}
