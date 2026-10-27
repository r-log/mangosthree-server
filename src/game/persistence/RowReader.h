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

#ifndef MANGOS_H_ROWREADER
#define MANGOS_H_ROWREADER

#include "Database/DatabaseEnv.h"
#include "persistence/TableRead.h"

#include <optional>
#include <string>
#include <vector>

/// Reads every row of a result with `read`, in order, and deletes the result; none for no result.
template<class Row, class Read>
std::vector<Row> ReadRows(QueryResult* result, Read read)
{
    std::vector<Row> rows;
    if (!result)
    {
        return rows;
    }

    rows.reserve(size_t(result->GetRowCount()));
    do
    {
        rows.push_back(read(result->Fetch()));
    }
    while (result->NextRow());

    delete result;
    return rows;
}

/// Reads the first row of a result with `read` and deletes the result; none for no result.
template<class Row, class Read>
std::optional<Row> ReadRow(QueryResult* result, Read read)
{
    if (!result)
    {
        return std::nullopt;
    }

    std::optional<Row> row = read(result->Fetch());
    delete result;
    return row;
}

/// Runs `sql` on the world database, synchronously, and reads every row with `read`. When the
/// select answers nothing, counts the rows of the same select (`SELECT COUNT(*) FROM (sql) AS
/// probe`) to tell an empty table from an unreadable one.
template<class Row, class Read>
TableRead<Row> ReadWorldTable(char const* sql, Read read)
{
    TableRead<Row> table;
    QueryResult* result = WorldDatabase.Query(sql);
    if (result)
    {
        table.status = TableReadStatus::Loaded;
        table.rows = ReadRows<Row>(result, read);
        return table;
    }

    std::string probe = std::string("SELECT COUNT(*) FROM (") + sql + ") AS probe";
    std::optional<uint64> count = ReadRow<uint64>(WorldDatabase.Query(probe.c_str()), [](Field* fields) { return fields[0].GetUInt64(); });
    if (!count)
    {
        table.status = TableReadStatus::QueryFailed;
    }
    else if (*count == 0)
    {
        table.status = TableReadStatus::Empty;
    }
    else
    {
        table.status = TableReadStatus::ChangedWhileRead;
    }
    return table;
}

#endif
