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

#ifndef MANGOS_H_SPAWNREPOSITORY
#define MANGOS_H_SPAWNREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// A creature spawn's saved row, in the order of the INSERT's columns, each value of the type it
/// is written as.
struct CreatureSpawnRow
{
    uint32 guid = 0;
    uint32 id = 0;
    uint32 map = 0;
    uint32 spawnMask = 0;
    uint32 phaseMask = 0;
    uint32 modelIdOverride = 0;
    int32 equipmentId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    uint32 spawnTimeSecs = 0;
    float spawnDist = 0.0f;
    uint32 currentWaypoint = 0;
    uint32 curHealth = 0;
    uint32 curMana = 0;
    int isDead = 0;
    uint32 movementType = 0;
};

/// A game object spawn's saved row, in the order of the INSERT's columns, each value of the type it
/// is written as.
struct GameObjectSpawnRow
{
    uint32 guid = 0;
    uint32 entry = 0;
    uint32 map = 0;
    uint32 spawnMask = 0;
    uint32 phaseMask = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    float rotationW = 0.0f;
    uint32 respawnDelay = 0;
    uint32 animProgress = 0;
    uint32 goState = 0;
};

/// The creature and game object spawns on the world database (`creature`, `gameobject` and the
/// tables keyed on a spawn), as a GM saves and deletes them. The start-up reads run
/// synchronously; every write is queued and written to the SQL log.
namespace SpawnRepository
{
    /// Writes a creature spawn's row over the old one, in one transaction.
    void ReplaceCreature(CreatureSpawnRow const& row);

    /// Deletes a creature spawn and every row keyed on it (addon, waypoints, game event data,
    /// battleground and linking), in one transaction.
    void DeleteCreature(uint32 guid);

    /// Writes a game object spawn's row over the old one, in one transaction.
    void ReplaceGameObject(GameObjectSpawnRow const& row);

    /// Deletes a game object spawn and its game event and battleground rows.
    void DeleteGameObject(uint32 guid);

    /// Selects the highest creature spawn guid, at start-up.
    QueryResult* QueryHighestCreatureGuid();

    /// Selects the highest game object spawn guid, at start-up.
    QueryResult* QueryHighestGameObjectGuid();

    /// Selects every creature spawn with its addon and event data, at start-up.
    QueryResult* QueryCreatureSpawns();

    /// Selects every game object spawn with its event data, at start-up.
    QueryResult* QueryGameObjectSpawns();
}

#endif
