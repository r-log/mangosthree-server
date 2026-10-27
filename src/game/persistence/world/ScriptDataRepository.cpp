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

#include "ScriptDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace ScriptDataRepository
{
    static ScriptBindingRow ReadScriptBinding(Field* fields)
    {
        ScriptBindingRow row;
        row.type = fields[0].GetUInt8();
        row.bind = fields[1].GetInt32();
        row.scriptName = fields[2].GetCppString();
        row.data = fields[3].GetUInt8();
        return row;
    }

    std::vector<ScriptBindingRow> QueryScriptBindings()
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`");
        return ReadRows<ScriptBindingRow>(result, ReadScriptBinding);
    }

    static ScriptNameRow ReadScriptName(Field* fields)
    {
        ScriptNameRow row;
        if (char const* text = fields[0].GetString())
        {
            row.scriptName = text;
        }
        return row;
    }

    std::vector<ScriptNameRow> QueryScriptNames()
    {
        QueryResult* result = WorldDatabase.Query("SELECT DISTINCT(`ScriptName`) FROM `script_binding`");
        return ReadRows<ScriptNameRow>(result, ReadScriptName);
    }

    static DbScriptRow ReadDbScript(Field* fields)
    {
        DbScriptRow row;
        row.id = fields[0].GetUInt32();
        row.delay = fields[1].GetUInt32();
        row.command = fields[2].GetUInt32();
        row.datalong = fields[3].GetUInt32();
        row.datalong2 = fields[4].GetUInt32();
        row.buddyEntry = fields[5].GetUInt32();
        row.searchRadius = fields[6].GetUInt32();
        row.dataFlags = fields[7].GetUInt8();
        row.dataint = fields[8].GetInt32();
        row.dataint2 = fields[9].GetInt32();
        row.dataint3 = fields[10].GetInt32();
        row.dataint4 = fields[11].GetInt32();
        row.x = fields[12].GetFloat();
        row.y = fields[13].GetFloat();
        row.z = fields[14].GetFloat();
        row.o = fields[15].GetFloat();
        return row;
    }

    std::vector<DbScriptRow> QueryDbScripts(int type)
    {
        //                                  0   1      2        3         4          5            6              7           8        9         10        11        12 13 14 15
        QueryResult* result = WorldDatabase.PQuery("SELECT `id`, `delay`, `command`, `datalong`, `datalong2`, `buddy_entry`, `search_radius`, `data_flags`, `dataint`, `dataint2`, `dataint3`, `dataint4`, `x`, `y`, `z`, `o` FROM `db_scripts` WHERE `script_type` = %d ORDER BY `script_guid` ASC", type);
        return ReadRows<DbScriptRow>(result, ReadDbScript);
    }

    std::vector<uint32> QueryScriptWaypointCounts()
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT COUNT(`entry`) FROM `script_waypoint` GROUP BY `entry`");
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static ScriptWaypointRow ReadScriptWaypoint(Field* fields)
    {
        ScriptWaypointRow row;
        row.entry = fields[0].GetUInt32();
        row.pointId = fields[1].GetUInt32();
        row.positionX = fields[2].GetFloat();
        row.positionY = fields[3].GetFloat();
        row.positionZ = fields[4].GetFloat();
        row.waitTime = fields[5].GetUInt32();
        return row;
    }

    std::vector<ScriptWaypointRow> QueryScriptWaypoints()
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `pointid`, `location_x`, `location_y`, `location_z`, `waittime` FROM `script_waypoint` ORDER BY `pointid`");
        return ReadRows<ScriptWaypointRow>(result, ReadScriptWaypoint);
    }
}
