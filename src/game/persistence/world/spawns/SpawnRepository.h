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

#include <optional>
#include <set>
#include <string>
#include <vector>

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

/// One creature spawn as the startup load reads it: its `creature` row, its game event, and the
/// pools of its guid and of its entry.
struct CreatureSpawnDataRow
{
    uint32 guid = 0;
    uint32 id = 0;
    uint32 map = 0;
    uint32 modelid = 0;
    uint32 equipmentId = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint32 spawntimesecs = 0;
    float spawndist = 0.0f;
    uint32 currentwaypoint = 0;
    uint32 curhealth = 0;
    uint32 curmana = 0;
    bool deathState = false;
    uint8 movementType = 0;
    uint8 spawnMask = 0;
    uint32 phaseMask = 0;
    int16 gameEvent = 0;
    int16 guidPoolId = 0;
    int16 entryPoolId = 0;
};

/// One game object spawn as the startup load reads it: its `gameobject` row, its game event, and
/// the pools of its guid and of its entry.
struct GameObjectSpawnDataRow
{
    uint32 guid = 0;
    uint32 id = 0;
    uint32 map = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    float rotation0 = 0.0f;
    float rotation1 = 0.0f;
    float rotation2 = 0.0f;
    float rotation3 = 0.0f;
    int32 spawntimesecs = 0;
    uint32 animprogress = 0;
    uint32 state = 0;
    uint8 spawnMask = 0;
    uint32 phaseMask = 0;
    int16 gameEvent = 0;
    int16 guidPoolId = 0;
    int16 entryPoolId = 0;
};

/// A creature spawn's entry and map.
struct CreatureEntryAndMapRow
{
    uint32 id = 0;
    uint32 map = 0;
};

/// One spawn of an entry as a GM's object or creature list shows it: its guid, position and map.
struct SpawnListRow
{
    uint32 guid = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    uint16 map = 0;
};

/// A game object spawn near a GM: its guid, entry, position and map.
struct GameObjectInRangeRow
{
    uint32 guid = 0;
    uint32 entry = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    uint16 map = 0;
};

/// A game object spawn a GM's target command finds, nearest first: its guid, entry, position,
/// orientation and map.
struct NearestGameObjectRow
{
    uint32 guid = 0;
    uint32 entry = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint16 map = 0;
};

/// The creature and game object spawns on the world database (`creature`, `gameobject` and the
/// tables keyed on a spawn), as a GM saves and deletes them. The start-up reads and a GM's spawn
/// lists and look-ups run synchronously; every write is queued and written to the SQL log.
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
    std::optional<uint32> QueryHighestCreatureGuid();

    /// Selects the highest game object spawn guid, at start-up.
    std::optional<uint32> QueryHighestGameObjectGuid();

    /// Selects every creature spawn with its addon and event data, at start-up.
    std::vector<CreatureSpawnDataRow> QueryCreatureSpawns();

    /// Selects every game object spawn with its event data, at start-up.
    std::vector<GameObjectSpawnDataRow> QueryGameObjectSpawns();

    /// Selects a creature spawn's entry and map.
    std::optional<CreatureEntryAndMapRow> QueryCreatureEntryAndMap(uint32 guid);

    /// Selects up to two spawn guids of a creature entry on a map.
    std::vector<uint32> QueryCreatureGuidsOnMap(uint32 entry, uint32 mapId);

    /// Selects how many game object spawns are of an entry, for a GM's list.
    std::optional<uint32> QueryGameObjectCountOf(uint32 entry);

    /// Selects up to count game object spawns of an entry, nearest a position first.
    std::vector<SpawnListRow> QueryGameObjectsNear(float x, float y, float z, uint32 entry, uint32 count);

    /// Selects up to count game object spawns of an entry.
    std::vector<SpawnListRow> QueryGameObjectsOf(uint32 entry, uint32 count);

    /// Selects how many creature spawns are of an entry, for a GM's list.
    std::optional<uint32> QueryCreatureCountOf(uint32 entry);

    /// Selects up to count creature spawns of an entry, nearest a position first.
    std::vector<SpawnListRow> QueryCreaturesNear(float x, float y, float z, uint32 entry, uint32 count);

    /// Selects up to count creature spawns of an entry.
    std::vector<SpawnListRow> QueryCreaturesOf(uint32 entry, uint32 count);

    /// Writes a GM's new position of a creature spawn.
    void SetCreaturePosition(float x, float y, float z, float o, uint32 guid);

    /// Writes a GM's new wander distance and movement type of a creature spawn.
    void SetCreatureSpawnDistance(float distance, int32 movementType, uint32 guid);

    /// Writes a GM's new respawn time of a creature spawn.
    void SetCreatureSpawnTime(uint32 seconds, uint32 guid);

    /// Selects the game object spawns on a map within a squared distance of a position, nearest first.
    std::vector<GameObjectInRangeRow> QueryGameObjectsInRange(float x, float y, float z, uint32 mapId, float distanceSquared);

    /// Selects the nearest game object spawn of an entry on a map.
    std::vector<NearestGameObjectRow> QueryNearestGameObjectOf(float x, float y, float z, uint32 mapId, uint32 entry);

    /// Selects the nearest game object spawn on a map whose template name contains a text, the text
    /// escaped first.
    std::vector<NearestGameObjectRow> QueryNearestGameObjectNamed(float x, float y, float z, uint32 mapId, std::string name);

    /// Selects the ten nearest game object spawns on a map that belong to no event or to one of the
    /// active events.
    std::vector<NearestGameObjectRow> QueryNearestGameObjects(float x, float y, float z, uint32 mapId, std::set<uint16> const& activeEvents);

    /// Selects the creature spawns whose template name contains a text, the text escaped first.
    std::vector<uint32> QueryCreatureGuidsNamed(std::string name);

    /// Selects the game object spawns whose template name contains a text, the text escaped first; the
    /// column is spelled as it always was.
    std::vector<uint32> QueryGameObjectGuidsNamed(std::string name);

    /// Selects how many creature spawns are of an entry, for a GM's waypoint commands.
    std::optional<uint32> QueryCreatureIdCountOf(uint32 entry);
}

#endif
