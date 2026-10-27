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

/// PageTextRepository's read on the global WorldDatabase backed by fakes: the statement names its
/// columns, every column of a canned row lands in its member, a NULL text reads as "", and no row
/// answers an empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/items/PageTextRepository.h"

#include <vector>

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

// Each column of a `page_text` row lands in its member, and the rows come back in order.
TEST(PageTextRepository_PageTexts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002"},
        FakeRow{"2000", "s2.1", "2002"},
    });
    TableRead<PageTextRow> read = PageTextRepository::QueryPageTexts();
    std::vector<PageTextRow> const& rows = read.rows;
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `text`, `next_page` FROM `page_text`");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].text, "s1.1");
    CHECK_EQ(rows[0].nextPage, 1002u);
    CHECK_EQ(rows[1].entry, 2000u);
    CHECK_STR(rows[1].text, "s2.1");
    CHECK_EQ(rows[1].nextPage, 2002u);
}

// A NULL text reads as an empty string, and a NULL next page as 0. The first row sets the
// result's width, so the short second row's missing columns read as NULL.
TEST(PageTextRepository_NullTextReadsAsEmpty)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"8", "s8.1", "9"},
        FakeRow{"7"},
    });
    TableRead<PageTextRow> read = PageTextRepository::QueryPageTexts();
    std::vector<PageTextRow> const& rows = read.rows;
    REQUIRE(rows.size() == 2);
    CHECK_STR(rows[0].text, "s8.1");
    CHECK_EQ(rows[1].entry, 7u);
    CHECK_STR(rows[1].text, "");
    CHECK_EQ(rows[1].nextPage, 0u);
}

// No row and a count of 0 answer an empty table; the count is over the same select.
TEST(PageTextRepository_NoRowsAnswerEmpty)
{
    Db db;
    db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
    TableRead<PageTextRow> read = PageTextRepository::QueryPageTexts();
    CHECK(read.status == TableReadStatus::Empty);
    CHECK(read.rows.empty());
    REQUIRE(db.query.executed.size() == 2);
    CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `entry`, `text`, `next_page` FROM `page_text`) AS probe");
}

// No row and no count answer an unreadable table.
TEST(PageTextRepository_FailedQueryAnswersUnreadable)
{
    Db db;
    TableRead<PageTextRow> read = PageTextRepository::QueryPageTexts();
    CHECK(read.status == TableReadStatus::QueryFailed);
    CHECK(read.Unreadable());
}
