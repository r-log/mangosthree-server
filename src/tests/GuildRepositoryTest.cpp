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
