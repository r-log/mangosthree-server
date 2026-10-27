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

/// The mails' repository on the global CharacterDatabase backed by fakes: every statement the mail
/// flow writes, in order, the send queued as one transaction around the mail and its items, the
/// delete as one of its own, and the two asynchronous reads queued with their text and answered on
/// the world thread.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/mail/MailRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    /// A canned result as a connection hands it out: on its first row.
    QueryResult* CannedResult(FakeRows rows)
    {
        FakeQueryResult* result = new FakeQueryResult(std::move(rows));
        result->NextRow();
        return result;
    }

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

// A send: the mail's row and one row per item between CharacterTransaction's Begin and Commit
// reach the database as one queued transaction, the subject and body bound.
TEST(MailRepository_SendJoinsOneTransaction)
{
    Db db;
    MailRow row;
    row.id = 900;
    row.messageType = 0;
    row.stationery = 41;
    row.mailTemplateId = 0;
    row.sender = 7;
    row.receiver = 42;
    row.subject = "Hello";
    row.body = "Two potions";
    row.hasItems = 1;
    row.expireTime = 1702592000;
    row.deliverTime = 1700000000;
    row.money = 5000;
    row.cod = 0;
    row.checked = 16;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        MailRepository::InsertMail(row);
        MailRepository::InsertMailItem(900, 55, 118, 42);
        MailRepository::InsertMailItem(900, 56, 118, 42);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "INSERT INTO `mail` (`id`,`messageType`,`stationery`,`mailTemplateId`,`sender`,`receiver`,`subject`,`body`,`has_items`,`expire_time`,`deliver_time`,`money`,`cod`,`checked`) "
        "VALUES ('900','0','41','0','7','42','Hello','Two potions','1','1702592000','1700000000','5000','0','16')",
        "INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('900', '55', '118','42')",
        "INSERT INTO `mail_items` (`mail_id`,`item_guid`,`item_template`,`receiver`) VALUES ('900', '56', '118','42')",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The delete is one queued transaction of its own; the other writes are one statement each.
TEST(MailRepository_DeleteAndSingleWrites)
{
    Db db;
    {
        TickGuard::Scope scope;
        MailRepository::DeleteMail(900);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `mail` WHERE `id` = '900'",
        "DELETE FROM `mail_items` WHERE `mail_id` = '900'",
    }, __LINE__);

    MailRepository::SetHasItems(901);
    MailRepository::SetMailedItemOwner(55, 42);
    MailRepository::DeleteDroppedItem(56);
    CheckLines(db.Take(), {
        "UPDATE `mail` SET `has_items` = 1 WHERE `id` = 901",
        "UPDATE `item_instance` SET `owner_guid` = '42' WHERE `guid`='55'",
        "DELETE FROM `item_instance` WHERE `guid`='56'",
    }, __LINE__);
}

// The two reads are queued, not run on the calling thread, and each callback runs once when the
// world thread drains the result queue.
TEST(MailRepository_ReadsAreAsynchronous)
{
    Db db;
    int answered = 0;
    {
        TickGuard::Scope scope;
        MailRepository::QueryMailboxCount(42, [&answered](std::optional<uint32> /*count*/) { ++answered; });
        MailRepository::QueryMassMailReceivers("SELECT `guid` FROM `characters` WHERE `level` > 80",
                                               [&answered](std::vector<uint32> /*guids*/) { ++answered; });
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    }
    CheckLines(db.Take(), {
        "SELECT COUNT(*) FROM `mail` WHERE `receiver` = '42'",
        "SELECT `guid` FROM `characters` WHERE `level` > 80",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 2);
}

// The mailbox count answers the count, and the mass mail read the guids in order.
TEST(MailRepository_AsyncValues)
{
    Db db;
    db.async.Answer("SELECT COUNT(*) FROM `mail`", FakeRows{FakeRow{"1007"}});
    db.async.Answer("SELECT `guid` FROM `characters`", FakeRows{FakeRow{"1000"}, FakeRow{"2000"}});
    std::optional<uint32> count;
    std::vector<uint32> guids;
    MailRepository::QueryMailboxCount(42, [&count](std::optional<uint32> got) { count = got; });
    MailRepository::QueryMassMailReceivers("SELECT `guid` FROM `characters` WHERE `level` > 80", [&guids](std::vector<uint32> got) { guids = got; });
    db.Take();
    CharacterDatabase.ProcessResultQueue();
    REQUIRE(count);
    CHECK_EQ(*count, 1007u);
    REQUIRE(guids.size() == 2);
    CHECK_EQ(guids[0], 1000u);
    CHECK_EQ(guids[1], 2000u);
}

// Each column of an expired mail and of its items lands in its member, and the rows come back in
// order.
TEST(MailRepository_ExpiredRows)
{
    SqlQueryHolder holder;
    holder.SetSize(EXPIRED_MAIL_COUNT);
    holder.SetResult(EXPIRED_MAIL_ROWS, CannedResult(FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "1", "4294967301", "1006", "1007", "-1008"},
        FakeRow{"2000", "201", "2002", "2003", "0", "8589934597", "2006", "2007", "-2008"},
    }));
    holder.SetResult(EXPIRED_MAIL_ITEMS, CannedResult(FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    }));
    std::vector<ExpiredMailRow> mails = MailRepository::ExpiredMails(&holder);
    REQUIRE(mails.size() == 2);
    CHECK_EQ(mails[0].id, 1000u);
    CHECK_EQ(mails[0].messageType, uint8(101));
    CHECK_EQ(mails[0].sender, 1002u);
    CHECK_EQ(mails[0].receiver, 1003u);
    CHECK_EQ(mails[0].hasItems, true);
    CHECK_EQ(mails[0].expireTime, uint64(4294967301ull));
    CHECK_EQ(mails[0].cod, 1006u);
    CHECK_EQ(mails[0].checked, 1007u);
    CHECK_EQ(mails[0].mailTemplateId, int16(-1008));
    CHECK_EQ(mails[1].id, 2000u);
    CHECK_EQ(mails[1].hasItems, false);
    std::vector<ExpiredMailItemRow> items = MailRepository::ExpiredMailItems(&holder);
    REQUIRE(items.size() == 2);
    CHECK_EQ(items[0].itemGuid, 1000u);
    CHECK_EQ(items[0].itemTemplate, 1001u);
    CHECK_EQ(items[0].mailId, 1002u);
    CHECK_EQ(items[1].itemGuid, 2000u);
}
