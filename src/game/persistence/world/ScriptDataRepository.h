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

#ifndef MANGOS_H_SCRIPTDATAREPOSITORY
#define MANGOS_H_SCRIPTDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

class QueryResult;

/// One `script_binding` row.
struct ScriptBindingRow
{
    uint8 type = 0;
    int32 bind = 0;
    std::string scriptName;
    uint8 data = 0;
};

/// One `script_binding` row.
struct ScriptNameRow
{
    std::optional<std::string> scriptName;
};

/// One `db_scripts` row.
struct DbScriptRow
{
    uint32 id = 0;
    uint32 delay = 0;
    uint32 command = 0;
    uint32 datalong = 0;
    uint32 datalong2 = 0;
    uint32 buddyEntry = 0;
    uint32 searchRadius = 0;
    uint8 dataFlags = 0;
    int32 dataint = 0;
    int32 dataint2 = 0;
    int32 dataint3 = 0;
    int32 dataint4 = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
};

/// The script data on the world database (`script_binding`, `db_scripts`, `script_waypoint`): the
/// script manager's and the script library's loads at startup and on a reload, each run
/// synchronously.
namespace ScriptDataRepository
{
    /// Selects every script binding (`script_binding`).
    std::vector<ScriptBindingRow> QueryScriptBindings();

    /// Selects every script name a binding names.
    std::vector<ScriptNameRow> QueryScriptNames();

    /// Selects every database script step of one script type, in order.
    std::vector<DbScriptRow> QueryDbScripts(int type);

    /// Selects the number of script waypoints of every creature entry.
    QueryResult* QueryScriptWaypointCounts();

    /// Selects every script waypoint, ordered by point.
    QueryResult* QueryScriptWaypoints();
}

#endif
