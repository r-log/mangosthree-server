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

/// ReadRows and ReadRow over a canned result: no result reads as none, rows are read in order, and
/// ReadRow reads the first row only.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "persistence/RowReader.h"

#include <optional>
#include <vector>

namespace
{
    /// A canned result as a connection hands it out: on its first row.
    QueryResult* Result(FakeRows rows)
    {
        FakeQueryResult* result = new FakeQueryResult(std::move(rows));
        result->NextRow();
        return result;
    }

    uint32 ReadFirst(Field* fields)
    {
        return fields[0].GetUInt32();
    }
}

TEST(RowReader_NoResultReadsAsNone)
{
    CHECK(ReadRows<uint32>(nullptr, ReadFirst).empty());
    CHECK(!ReadRow<uint32>(nullptr, ReadFirst));
}

TEST(RowReader_ReadRowsReadsEveryRowInOrder)
{
    QueryResult* result = Result(FakeRows{FakeRow{"7"}, FakeRow{"8"}, FakeRow{"9"}});
    std::vector<uint32> rows = ReadRows<uint32>(result, ReadFirst);
    REQUIRE(rows.size() == 3);
    CHECK_EQ(rows[0], 7u);
    CHECK_EQ(rows[1], 8u);
    CHECK_EQ(rows[2], 9u);
}

TEST(RowReader_ReadRowReadsTheFirstRow)
{
    QueryResult* result = Result(FakeRows{FakeRow{"7"}, FakeRow{"8"}});
    std::optional<uint32> row = ReadRow<uint32>(result, ReadFirst);
    REQUIRE(row);
    CHECK_EQ(*row, 7u);
}