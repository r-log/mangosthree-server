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

/// The client settings' repository on the global CharacterDatabase backed by fakes: the two loads
/// run on the calling thread's query connection; the writes are queued, each data block's DELETE
/// and INSERT as one transaction, with their values substituted as the server would send them to
/// MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/account/AccountDataRepository.h"

#include <optional>
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

// The account's data blocks and its tutorial flags are read on the calling thread.
TEST(AccountDataRepository_LoadsAreSynchronous)
{
    Db db;
    AccountDataRepository::QueryGlobal(17);
    AccountDataRepository::QueryTutorials(17);
    CheckLines(db.query.executed, {
        "SELECT `type`, `time`, `data` FROM `account_data` WHERE `account` = '17'",
        "SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '17'",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// An account's and a character's data block each replace the old one in one transaction; the
// tutorial flags are an UPDATE or an INSERT, the account last in one and first in the other.
TEST(AccountDataRepository_Writes)
{
    Db db;
    uint32 const flags[TUTORIAL_ROW_FLAGS] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    {
        TickGuard::Scope scope;
        AccountDataRepository::ReplaceGlobal(17, 2, 1700000000, "macros");
        AccountDataRepository::ReplaceCharacter(42, 5, 1700000001, "layout");
        AccountDataRepository::UpdateTutorials(17, flags);
        AccountDataRepository::InsertTutorials(18, flags);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(4));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `account_data` WHERE `account` = '17' AND `type` = '2'",
        "INSERT INTO `account_data` VALUES ('17','2','1700000000','macros')",
        "DELETE FROM `character_account_data` WHERE `guid` = '42' AND `type` = '5'",
        "INSERT INTO `character_account_data` VALUES ('42','5','1700000001','layout')",
        "UPDATE `character_tutorial` SET `tut0`='1', `tut1`='2', `tut2`='3', `tut3`='4', `tut4`='5', `tut5`='6', `tut6`='7', `tut7`='8' WHERE `account` = '17'",
        "INSERT INTO `character_tutorial` (`account`,`tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7`) VALUES ('18', '1', '2', '3', '4', '5', '6', '7', '8')",
    }, __LINE__);
}

// An account's delete deletes its tutorial flags.
TEST(AccountDataRepository_DeleteTutorials)
{
    Db db;
    {
        TickGuard::Scope scope;
        AccountDataRepository::DeleteTutorials(17);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "DELETE FROM `character_tutorial` WHERE `account` = '17'",
    }, __LINE__);
}

// Each column of a saved data block of the client lands in its member, and the rows come back in
// order.
TEST(AccountDataRepository_Global)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "4294967297", "s1.2"},
        FakeRow{"2000", "8589934593", "s2.2"},
    });
    std::vector<AccountDataRow> rows = AccountDataRepository::QueryGlobal(17);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].type, 1000u);
    CHECK_EQ(rows[0].time, uint64(4294967297ull));
    CHECK_STR(rows[0].data, "s1.2");
    CHECK_EQ(rows[1].type, 2000u);
}

// Each tutorial flag lands in its member, and no rows answer nothing.
TEST(AccountDataRepository_Tutorials)
{
    {
        Db db;
        CHECK(!AccountDataRepository::QueryTutorials(17));
    }
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005", "2006", "2007"},
    });
    std::optional<TutorialRow> row = AccountDataRepository::QueryTutorials(17);
    REQUIRE(row);
    CHECK_EQ(row->flags[0], 1000u);
    CHECK_EQ(row->flags[1], 1001u);
    CHECK_EQ(row->flags[2], 1002u);
    CHECK_EQ(row->flags[3], 1003u);
    CHECK_EQ(row->flags[4], 1004u);
    CHECK_EQ(row->flags[5], 1005u);
    CHECK_EQ(row->flags[6], 1006u);
    CHECK_EQ(row->flags[7], 1007u);
}
