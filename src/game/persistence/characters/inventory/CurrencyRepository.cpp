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

#include "CurrencyRepository.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "persistence/RowReader.h"

namespace CurrencyRepository
{
    CurrencyRow ReadRow(Field* fields)
    {
        //         0   1           2          4            5
        // "SELECT id, totalCount, weekCount, seasonCount, flags FROM character_currencies WHERE guid = '%u'"

        CurrencyRow row;
        row.id          = fields[0].GetUInt16();
        row.totalCount  = fields[1].GetUInt32();
        row.weekCount   = fields[2].GetUInt32();
        row.seasonCount = fields[3].GetUInt32();
        row.flags       = fields[4].GetUInt8();
        return row;
    }

    std::vector<CurrencyRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        return ReadRows<CurrencyRow>(holder->GetResult(slot), ReadRow);
    }

    void Update(uint32 ownerGuidLow, CurrencyRow const& row)
    {
        CharacterDatabase.PExecute("UPDATE `character_currencies` SET `totalCount` = '%u', `weekCount` = '%u', `seasonCount` = '%u', `flags` = '%u' WHERE `guid` = '%u' AND `id` = '%u'", row.totalCount, row.weekCount, row.seasonCount, row.flags, ownerGuidLow, row.id);
    }

    void Insert(uint32 ownerGuidLow, CurrencyRow const& row)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_currencies` (`guid`, `id`, `totalCount`, `weekCount`, `seasonCount`, `flags`) VALUES ('%u', '%u', '%u', '%u', '%u', '%u')", ownerGuidLow, row.id, row.totalCount, row.weekCount, row.seasonCount, row.flags);
    }

    void ResetEveryWeekCount()
    {
        CharacterDatabase.Execute("UPDATE `character_currencies` SET `weekCount` = 0");
    }
}
