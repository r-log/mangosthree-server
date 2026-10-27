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

#include "CharacterDumpRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CharacterDumpRepository
{
    std::vector<std::string> QueryColumnNames(char const* table)
    {
        std::vector<std::string> names;
        QueryNamedResult* result = CharacterDatabase.PQueryNamed("SELECT * FROM `%s` LIMIT 1", table);
        if (!result)
        {
            return names;
        }

        QueryFieldNames const& fieldNames = result->GetFieldNames();
        names.assign(fieldNames.begin(), fieldNames.end());
        delete result;
        return names;
    }

    std::vector<DumpRow> QueryRows(char const* table, std::string const& columns, std::string const& where)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT %s FROM `%s` WHERE %s", columns.c_str(), table, where.c_str());
        uint32 fieldCount = result ? result->GetFieldCount() : 0;
        return ReadRows<DumpRow>(result, [fieldCount](Field* fields)
                                 {
                                     DumpRow row;
                                     for (uint32 i = 0; i < fieldCount; ++i)
                                     {
                                         if (fields[i].IsNULL())
                                         {
                                             row.values.push_back(std::nullopt);
                                         }
                                         else
                                         {
                                             row.values.push_back(fields[i].GetCppString());
                                         }
                                     }
                                     return row;
                                 });
    }

    static CharacterDbVersionRow ReadVersion(Field* fields)
    {
        CharacterDbVersionRow row;
        row.version = fields[0].GetInt16();
        row.structure = fields[1].GetInt16();
        row.description = fields[2].GetCppString();
        row.comment = fields[3].GetCppString();
        return row;
    }

    std::optional<CharacterDbVersionRow> QueryVersion()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `version`, `structure`, `description`, `comment` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` ASC LIMIT 1");
        return ReadRow<CharacterDbVersionRow>(result, ReadVersion);
    }

    bool QueryGuidInUse(uint32 guid)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT * FROM `characters` WHERE `guid` = '%u'", guid);
        bool found = result != NULL;
        delete result;
        return found;
    }

    bool QueryNameInUse(std::string const& name)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT * FROM `characters` WHERE `name` = '%s'", name.c_str());
        bool found = result != NULL;
        delete result;
        return found;
    }

    void EscapeString(std::string& text)
    {
        CharacterDatabase.escape_string(text);
    }

    bool Execute(std::string const& sql)
    {
        return CharacterDatabase.Execute(sql.c_str());
    }
}
