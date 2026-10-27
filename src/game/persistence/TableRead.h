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

#ifndef MANGOS_H_TABLEREAD
#define MANGOS_H_TABLEREAD

#include <vector>

/// How a whole-table read on the world database ended.
enum class TableReadStatus
{
    Loaded,             ///< the select answered rows
    Empty,              ///< the select answered nothing, and the same select counts no row
    QueryFailed,        ///< the select and its count both failed: a missing table, or a missing or renamed column
    ChangedWhileRead    ///< the select answered nothing, then the same select counted rows
};

/// A whole-table read: how it ended and, when loaded, the rows in the order the database returned them.
template<class Row>
struct TableRead
{
    TableReadStatus status = TableReadStatus::Empty;
    std::vector<Row> rows;

    /// True when the table could not be read: the select failed or the table changed during the read.
    bool Unreadable() const
    {
        return status == TableReadStatus::QueryFailed || status == TableReadStatus::ChangedWhileRead;
    }
};

/// Why a read was unreadable, for the error line; "" when it was not.
inline char const* TableReadReason(TableReadStatus status)
{
    switch (status)
    {
        case TableReadStatus::QueryFailed:
            return "the query failed (a missing table, or a missing or renamed column)";
        case TableReadStatus::ChangedWhileRead:
            return "the query answered no row, then counted rows (the table changed during the read)";
        default:
            return "";
    }
}

#endif
