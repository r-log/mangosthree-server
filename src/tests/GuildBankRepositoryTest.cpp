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

/// The guild banks' repository on the global CharacterDatabase backed by fakes: every queued write
/// in order with its values substituted as the server would send them to MySQL after
/// ExecuteQueuedForTest(); a new tab is one queued transaction, and an item move's writes join the
/// transaction the bank opens around them.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/social/GuildBankRepository.h"

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

// A new tab is one queued transaction; an item move's event and slot writes join the transaction
// the bank opens around them.
TEST(GuildBankRepository_Transactions)
{
    Db db;
    {
        TickGuard::Scope scope;
        GuildBankRepository::CreateTab(3, 1);
        CharacterTransaction::Begin();
        GuildBankRepository::DeleteEventLogEntry(3, 4, 1);
        GuildBankRepository::InsertEventLogEntry(3, 4, 1, 3, 7, 2589, 20, 0, 1700000000);
        GuildBankRepository::DeleteItem(3, 1, 5);
        GuildBankRepository::StoreItem(3, 1, 6, 55, 2589);
        GuildBankRepository::SetMemberRemainingSlots(3, 7, 1, 9);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `guild_bank_tab` WHERE `guildid`='3' AND `TabId`='1'",
        "INSERT INTO `guild_bank_tab` (`guildid`,`TabId`) VALUES ('3','1')",
        "DELETE FROM `guild_bank_eventlog` WHERE `guildid`='3' AND `LogGuid`='4' AND `TabId`='1'",
        "INSERT INTO `guild_bank_eventlog` (`guildid`,`LogGuid`,`TabId`,`EventType`,`PlayerGuid`,`ItemOrMoney`,`ItemStackCount`,`DestTabId`,`TimeStamp`) VALUES ('3','4','1','3','7','2589','20','0','1700000000')",
        "DELETE FROM `guild_bank_item` WHERE `guildid`='3' AND `TabId`='1' AND `SlotId`='5'",
        "DELETE FROM `guild_bank_item` WHERE `guildid` = '3' AND `TabId` = '1'AND `SlotId` = '6'",
        "INSERT INTO `guild_bank_item` (`guildid`,`TabId`,`SlotId`,`item_guid`,`item_entry`) VALUES ('3', '1', '6', '55', '2589')",
        "UPDATE `guild_member` SET `BankRemSlotsTab1`='9' WHERE `guildid`='3' AND `guid`='7'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The money, allowance, right and tab writes, each its own call, in the argument order their text
// names them.
TEST(GuildBankRepository_SingleWrites)
{
    Db db;
    GuildBankRepository::SetTabInfo(3, 1, "Mats", "INV_Box_01");
    GuildBankRepository::SetTabText(3, 1, "Raid mats only");
    GuildBankRepository::SetBankMoney(3, 125000);
    GuildBankRepository::SetMemberRemainingMoney(3, 7, 500);
    GuildBankRepository::ResetMemberSlots(3, 7, 2, 28333333, 10);
    GuildBankRepository::ResetMemberMoney(3, 7, 28333333, 1000);
    GuildBankRepository::SetRankMoneyPerDay(3, 4, 2000);
    GuildBankRepository::DeleteBadRight(3, 9, 2);
    GuildBankRepository::SetRight(3, 2, 4, 3, 10);
    GuildBankRepository::DeleteOrphanRights(2);
    GuildBankRepository::MoveEventToMoneyTab(3, 1, 8, 100);
    CheckLines(db.Take(), {
        "UPDATE `guild_bank_tab` SET `TabName`='Mats',`TabIcon`='INV_Box_01' WHERE `guildid`='3' AND `TabId`='1'",
        "UPDATE `guild_bank_tab` SET `TabText`='Raid mats only' WHERE `guildid`='3' AND `TabId`='1'",
        "UPDATE `guild` SET `BankMoney`='125000' WHERE `guildid`='3'",
        "UPDATE `guild_member` SET `BankRemMoney`='500' WHERE `guildid`='3' AND `guid`='7'",
        "UPDATE `guild_member` SET `BankResetTimeTab2`='28333333', `BankRemSlotsTab2`='10' WHERE `guildid`='3' AND `guid`='7'",
        "UPDATE `guild_member` SET `BankResetTimeMoney`='28333333', `BankRemMoney`='1000' WHERE `guildid`='3' AND `guid`='7'",
        "UPDATE `guild_rank` SET `BankMoneyPerDay`='2000' WHERE `rid`='4' AND `guildid`='3'",
        "UPDATE `guild_member` SET `BankResetTimeMoney`='0' WHERE `guildid`='3' AND `rank`='4'",
        "DELETE FROM `guild_bank_right` WHERE `guildid`='3' AND `rid`='9' AND `TabId`='2'",
        "DELETE FROM `guild_bank_right` WHERE `guildid`='3' AND `TabId`='2' AND `rid`='4'",
        "INSERT INTO `guild_bank_right` (`guildid`,`TabId`,`rid`,`gbright`,`SlotPerDay`) VALUES ('3','2','4','3','10')",
        "UPDATE `guild_member` SET `BankResetTimeTab2`='0' WHERE `guildid`='3' AND `rank`='4'",
        "DELETE FROM `guild_bank_right` WHERE `guildid` = '2'",
        "UPDATE `guild_bank_eventlog` SET `TabId`='100' WHERE `guildid`='3' AND `TabId`='1' AND `LogGuid`='8'",
    }, __LINE__);
}

// Each column of a `guild_bank_tab` row lands in its member, and the rows come back in order.
TEST(GuildBankRepository_Tabs)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"100", "s1.1", "s1.2", "s1.3"},
        FakeRow{"200", "s2.1", "s2.2", "s2.3"},
    });
    std::vector<GuildBankTabRow> rows = GuildBankRepository::QueryTabs(1);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].tabId, uint8(100));
    CHECK_STR(rows[0].tabName, "s1.1");
    CHECK_STR(rows[0].tabIcon, "s1.2");
    CHECK_STR(rows[0].tabText, "s1.3");
    CHECK_EQ(rows[1].tabId, uint8(200));
}

// Each column of a `guild_bank_eventlog` row lands in its member, and the rows come back in order.
TEST(GuildBankRepository_EventLog)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "104", "105", "4294967302"},
        FakeRow{"2000", "201", "2002", "2003", "204", "205", "8589934598"},
    });
    std::vector<GuildBankEventLogRow> rows = GuildBankRepository::QueryEventLog(1, 1, 1);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].logGuid, 1000u);
    CHECK_EQ(rows[0].eventType, uint8(101));
    CHECK_EQ(rows[0].playerGuid, 1002u);
    CHECK_EQ(rows[0].itemOrMoney, 1003u);
    CHECK_EQ(rows[0].itemStackCount, uint8(104));
    CHECK_EQ(rows[0].destTabId, uint8(105));
    CHECK_EQ(rows[0].timeStamp, uint64(4294967302ull));
    CHECK_EQ(rows[1].logGuid, 2000u);
}
