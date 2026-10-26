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

#ifndef MANGOS_H_WAYPOINTREPOSITORY
#define MANGOS_H_WAYPOINTREPOSITORY

#include "Platform/Define.h"

#include <vector>

enum
{
    /// The texts a waypoint row holds (`textid1` to `textid5`).
    WAYPOINT_ROW_TEXTS = 5
};

/// The number of points of one creature's path (`creature_movement`).
struct PathPointCountRow
{
    uint32 id = 0;
    uint32 count = 0;
};

/// One `creature_movement` row: a point of a creature's path.
struct PathPointRow
{
    uint32 id = 0;
    uint32 point = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    uint32 waittime = 0;
    uint32 scriptId = 0;
    int32 textid[WAYPOINT_ROW_TEXTS] = {};
    uint32 emote = 0;
    uint32 spell = 0;
    float orientation = 0.0f;
    uint32 model1 = 0;
    uint32 model2 = 0;
};

/// The number of points of one creature template's path (`creature_movement_template`).
struct TemplatePathPointCountRow
{
    uint32 entry = 0;
    uint32 count = 0;
};

/// One `creature_movement_template` row: a point of a creature template's path.
struct TemplatePathPointRow
{
    uint32 entry = 0;
    uint32 point = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    uint32 waittime = 0;
    uint32 scriptId = 0;
    uint32 textid[WAYPOINT_ROW_TEXTS] = {};
    uint32 emote = 0;
    uint32 spell = 0;
    float orientation = 0.0f;
    uint32 model1 = 0;
    uint32 model2 = 0;
};

/// The creature waypoints on the world database (`creature_movement`,
/// `creature_movement_template`): the loads run at startup and on a reload, synchronously, into
/// rows, and the load's coordinate fixes are queued; a GM's path edits name the table and key
/// column they edit, and are queued and written to the SQL log.
namespace WaypointRepository
{
    /// Selects every guid path's point count.
    std::vector<PathPointCountRow> QueryPathPointCounts();

    /// Selects every point of every guid path.
    std::vector<PathPointRow> QueryPathPoints();

    /// Writes a guid path point's corrected position.
    void FixPathPoint(float x, float y, float z, uint32 id, uint32 point);

    /// Selects every template path's point count.
    std::vector<TemplatePathPointCountRow> QueryTemplatePathPointCounts();

    /// Selects every point of every template path.
    std::vector<TemplatePathPointRow> QueryTemplatePathPoints();

    /// Writes a template path point's corrected position.
    void FixTemplatePathPoint(float x, float y, uint32 entry, uint32 point);

    /// Moves a path's point one place on, as a GM's insert makes room.
    void ShiftPoint(char const* table, char const* keyField, uint32 key, uint32 point);

    /// Inserts a GM's new path point.
    void InsertPoint(char const* table, char const* keyField, uint32 key, uint32 point, float x, float y, float z);

    /// Deletes a GM's path point.
    void DeletePoint(char const* table, char const* keyField, uint32 key, uint32 point);

    /// Deletes a guid path.
    void DeletePath(uint32 id);

    /// Writes a GM's new position of a path point.
    void SetPointPosition(char const* table, float x, float y, float z, char const* keyField, uint32 key, uint32 point);

    /// Writes a GM's new wait time of a path point.
    void SetPointWaittime(char const* table, uint32 waittime, char const* keyField, uint32 key, uint32 point);

    /// Writes a GM's new orientation of a path point.
    void SetPointOrientation(char const* table, float orientation, char const* keyField, uint32 key, uint32 point);

    /// Writes a GM's new script of a path point.
    void SetPointScriptId(char const* table, uint32 scriptId, char const* keyField, uint32 key, uint32 point);
}

#endif
