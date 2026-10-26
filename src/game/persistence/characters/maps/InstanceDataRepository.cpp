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

#include "InstanceDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace InstanceDataRepository
{
    static InstanceDataRow ReadInstanceData(Field* fields)
    {
        InstanceDataRow row;
        row.id = fields[0].GetUInt32();
        row.map = fields[1].GetUInt32();
        if (char const* text = fields[2].GetString())
        {
            row.data = text;
        }
        return row;
    }

    std::vector<InstanceDataRow> QueryInstanceData()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `id`, `map`, `data` FROM `instance`");
        return ReadRows<InstanceDataRow>(result, ReadInstanceData);
    }

    static WorldMapDataRow ReadWorldMapData(Field* fields)
    {
        WorldMapDataRow row;
        row.map = fields[0].GetUInt32();
        if (char const* text = fields[1].GetString())
        {
            row.data = text;
        }
        return row;
    }

    std::vector<WorldMapDataRow> QueryWorldData()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `map`, `data` FROM `world`");
        return ReadRows<WorldMapDataRow>(result, ReadWorldMapData);
    }

    void SetInstanceData(std::string const& data, uint32 instanceId)
    {
        static SqlStatementID updInstanceData;
        SqlStatement update = CharacterDatabase.CreateStatement(updInstanceData,
                              "UPDATE `instance` SET `data` = ? WHERE `id` = ?");
        update.addString(data);
        update.addUInt32(instanceId);
        update.Execute();
    }

    void SetWorldData(std::string const& data, uint32 mapId)
    {
        static SqlStatementID updWorldData;
        SqlStatement update = CharacterDatabase.CreateStatement(updWorldData,
                              "UPDATE `world` SET `data` = ? WHERE `map` = ?");
        update.addString(data);
        update.addUInt32(mapId);
        update.Execute();
    }

    void InsertEmptyWorldData(uint32 mapId)
    {
        CharacterDatabase.PExecute("INSERT INTO `world` VALUES ('%u', '')", mapId);
    }
}
