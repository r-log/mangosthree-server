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

#ifndef MANGOS_H_CHARACTERDUMPREPOSITORY
#define MANGOS_H_CHARACTERDUMPREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One row of a dumped table: each column's text, none when it is NULL.
struct DumpRow
{
    std::vector<std::optional<std::string>> values;
};

/// The character database's newest version row, as a dump records it and a load checks it.
struct CharacterDbVersionRow
{
    int16 version = 0;
    int16 structure = 0;
    std::string description;
    std::string comment;
};

/// A character's dump as the character database sees it: the column names and rows of the
/// tables a dump writes, the version a dump records, and the checks a load makes before it
/// writes. Every read runs synchronously, on the thread of the GM command that asked for it; a
/// load runs its lines through Execute inside the CharacterTransaction it opens.
namespace CharacterDumpRepository
{
    /// Selects a table's column names, in order; none when the table has no rows.
    std::vector<std::string> QueryColumnNames(char const* table);

    /// Selects the rows of a table that a where clause names, the columns given, as text.
    std::vector<DumpRow> QueryRows(char const* table, std::string const& columns, std::string const& where);

    /// Selects the newest version row of the character database.
    std::optional<CharacterDbVersionRow> QueryVersion();

    /// Each answers whether a character holds a guid, or a name (escaped first by the caller).
    bool QueryGuidInUse(uint32 guid);
    bool QueryNameInUse(std::string const& name);

    /// Escapes text for the character database.
    void EscapeString(std::string& text);

    /// Executes one statement of a dump being loaded; false when it fails.
    bool Execute(std::string const& sql);
}

#endif
