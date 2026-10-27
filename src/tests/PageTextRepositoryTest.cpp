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
/// answers an empty list.

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
    std::vector<PageTextRow> rows = PageTextRepository::QueryPageTexts();
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

// A NULL text reads as an empty string, and a NULL next page as 0.
TEST(PageTextRepository_NullTextReadsAsEmpty)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"7"},
    });
    std::vector<PageTextRow> rows = PageTextRepository::QueryPageTexts();
    REQUIRE(rows.size() == 1);
    CHECK_EQ(rows[0].entry, 7u);
    CHECK_STR(rows[0].text, "");
    CHECK_EQ(rows[0].nextPage, 0u);
}

// No row answers an empty list.
TEST(PageTextRepository_NoRowsAnswerNothing)
{
    Db db;
    std::vector<PageTextRow> rows = PageTextRepository::QueryPageTexts();
    CHECK(rows.empty());
}
