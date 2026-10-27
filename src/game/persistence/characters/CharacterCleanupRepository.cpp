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

#include "CharacterCleanupRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

#include <sstream>

namespace CharacterCleanupRepository
{
    std::vector<uint32> QueryDistinct(char const* column, char const* table)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT DISTINCT `%s` FROM `%s`", column, table);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    void DeleteRowsWithIds(char const* table, char const* column, std::vector<uint32> const& ids)
    {
        std::ostringstream ss;
        ss << "DELETE FROM `" << table << "` WHERE `" << column << "` IN (";
        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (i)
            {
                ss << ",";
            }
            ss << ids[i];
        }
        ss << ")";
        CharacterDatabase.Execute(ss.str().c_str());
    }

    void DeleteTalentsPastLimits(uint32 maxSpecCount, uint32 maxRank)
    {
        CharacterDatabase.DirectPExecute("DELETE FROM `character_talent` WHERE `spec` > %u OR `current_rank` > %u", maxSpecCount, maxRank);
    }
}
