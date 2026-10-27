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

/// ObjectMgr::ReloadPageTexts on the global WorldDatabase backed by fakes: a reload that reads rows
/// replaces the pages and cuts a circular chain; a failed query, an empty table and a table that
/// changed during the read each keep the pages already loaded and answer how the read ended.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "persistence/TableRead.h"

#include <string>

namespace
{
    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };
}

TEST(PageTextReload_ReadRowsReplaceThePagesAndCutACycle)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"900001", "First page", "900002"},
        FakeRow{"900002", "Second page", "900001"},
    });
    CHECK(sObjectMgr.ReloadPageTexts() == TableReadStatus::Loaded);

    PageText const* first = sObjectMgr.GetPageText(900001);
    PageText const* second = sObjectMgr.GetPageText(900002);
    REQUIRE(first != NULL);
    REQUIRE(second != NULL);
    CHECK_STR(std::string(first->Text), "First page");
    CHECK_EQ(first->Next_Page, 900002u);
    CHECK_EQ(second->Next_Page, 0u);
}

TEST(PageTextReload_UnreadableOrEmptyKeepThePages)
{
    {
        Db db;
        db.query.Answer("SELECT `entry`", FakeRows{
            FakeRow{"900003", "Kept page", "0"},
        });
        CHECK(sObjectMgr.ReloadPageTexts() == TableReadStatus::Loaded);
    }

    {
        Db db;
        CHECK(sObjectMgr.ReloadPageTexts() == TableReadStatus::QueryFailed);
    }
    REQUIRE(sObjectMgr.GetPageText(900003) != NULL);
    CHECK_STR(std::string(sObjectMgr.GetPageText(900003)->Text), "Kept page");

    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"4"} });
        CHECK(sObjectMgr.ReloadPageTexts() == TableReadStatus::ChangedWhileRead);
    }
    CHECK(sObjectMgr.GetPageText(900003) != NULL);

    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        CHECK(sObjectMgr.ReloadPageTexts() == TableReadStatus::Empty);
    }
    CHECK(sObjectMgr.GetPageText(900003) != NULL);
}
