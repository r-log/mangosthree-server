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

/// ReadWorldTable on the global WorldDatabase backed by fakes: rows answer loaded with no second
/// query; no row answers the count of the same select, which tells an empty table (0), an
/// unreadable one (no answer) and one that changed during the read (rows) apart.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

#include <string>
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

    char const* const kSelect = "SELECT `id`, `name` FROM `some_table` WHERE `id` > 0";
    char const* const kProbe = "SELECT COUNT(*) FROM (SELECT `id`, `name` FROM `some_table` WHERE `id` > 0) AS probe";

    struct SomeRow
    {
        uint32 id = 0;
        std::string name;
    };

    SomeRow ReadSome(Field* fields)
    {
        SomeRow row;
        row.id = fields[0].GetUInt32();
        row.name = fields[1].GetCppString();
        return row;
    }
}

// Rows: loaded, in order, and the count is not asked.
TEST(TableRead_RowsAnswerLoaded)
{
    Db db;
    db.query.Answer("SELECT `id`", FakeRows{
        FakeRow{"5", "five"},
        FakeRow{"6", "six"},
    });
    TableRead<SomeRow> read = ReadWorldTable<SomeRow>(kSelect, ReadSome);
    CHECK(read.status == TableReadStatus::Loaded);
    CHECK(!read.Unreadable());
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].id, 5u);
    CHECK_STR(read.rows[1].name, "six");
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], kSelect);
}

// No row and a count of 0: empty, with the count taken over the same select.
TEST(TableRead_NoRowAndCountZeroAnswerEmpty)
{
    Db db;
    db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
    TableRead<SomeRow> read = ReadWorldTable<SomeRow>(kSelect, ReadSome);
    CHECK(read.status == TableReadStatus::Empty);
    CHECK(!read.Unreadable());
    CHECK(read.rows.empty());
    REQUIRE(db.query.executed.size() == 2);
    CHECK_STR(db.query.executed[0], kSelect);
    CHECK_STR(db.query.executed[1], kProbe);
    CHECK_STR(std::string(TableReadReason(read.status)), "");
}

// No row and no count either: the query failed, which is unreadable.
TEST(TableRead_NoRowAndNoCountAnswerQueryFailed)
{
    Db db;
    TableRead<SomeRow> read = ReadWorldTable<SomeRow>(kSelect, ReadSome);
    CHECK(read.status == TableReadStatus::QueryFailed);
    CHECK(read.Unreadable());
    CHECK(read.rows.empty());
    REQUIRE(db.query.executed.size() == 2);
    CHECK_STR(db.query.executed[1], kProbe);
    CHECK(std::string(TableReadReason(read.status)).find("the query failed") == 0);
}

// No row, then the same select counts rows: the table changed during the read, which is unreadable.
TEST(TableRead_NoRowThenRowsAnswerChangedWhileRead)
{
    Db db;
    db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"3"} });
    TableRead<SomeRow> read = ReadWorldTable<SomeRow>(kSelect, ReadSome);
    CHECK(read.status == TableReadStatus::ChangedWhileRead);
    CHECK(read.Unreadable());
    CHECK(read.rows.empty());
    CHECK(std::string(TableReadReason(read.status)).find("the table changed during the read") != std::string::npos);
}
