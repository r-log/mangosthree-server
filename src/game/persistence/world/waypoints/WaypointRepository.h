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

class QueryResult;

/// The creature waypoints on the world database (`creature_movement`, `creature_movement_template`):
/// the loads run at startup and on a reload, synchronously, for loaders that keep their loops, and the
/// load's coordinate fixes are queued; a GM's path edits name the table and key column they edit,
/// and are queued and written to the SQL log.
namespace WaypointRepository
{
    /// Selects every guid path's point count.
    QueryResult* QueryPathPointCounts();

    /// Selects every point of every guid path.
    QueryResult* QueryPathPoints();

    /// Writes a guid path point's corrected position.
    void FixPathPoint(float x, float y, float z, uint32 id, uint32 point);

    /// Selects every template path's point count.
    QueryResult* QueryTemplatePathPointCounts();

    /// Selects every point of every template path.
    QueryResult* QueryTemplatePathPoints();

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
