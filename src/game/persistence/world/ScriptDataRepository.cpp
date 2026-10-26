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

namespace ScriptDataRepository
{
    QueryResult* QueryScriptBindings()
    {
        return WorldDatabase.PQuery("SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`");
    }

    QueryResult* QueryScriptNames()
    {
        return WorldDatabase.Query("SELECT DISTINCT(`ScriptName`) FROM `script_binding`");
    }

    QueryResult* QueryDbScripts(int type)
    {
        //                                  0   1      2        3         4          5            6              7           8        9         10        11        12 13 14 15
        return WorldDatabase.PQuery("SELECT `id`, `delay`, `command`, `datalong`, `datalong2`, `buddy_entry`, `search_radius`, `data_flags`, `dataint`, `dataint2`, `dataint3`, `dataint4`, `x`, `y`, `z`, `o` FROM `db_scripts` WHERE `script_type` = %d ORDER BY `script_guid` ASC", type);
    }

    QueryResult* QueryScriptWaypointCounts()
    {
        return WorldDatabase.PQuery("SELECT COUNT(`entry`) FROM `script_waypoint` GROUP BY `entry`");
    }

    QueryResult* QueryScriptWaypoints()
    {
        return WorldDatabase.PQuery("SELECT `entry`, `pointid`, `location_x`, `location_y`, `location_z`, `waittime` FROM `script_waypoint` ORDER BY `pointid`");
    }
}
