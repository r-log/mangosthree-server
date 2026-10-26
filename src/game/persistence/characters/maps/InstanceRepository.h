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

#ifndef MANGOS_H_INSTANCEREPOSITORY
#define MANGOS_H_INSTANCEREPOSITORY

#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"

/// One creature respawn time (`creature_respawn`), with its instance's map, difficulty, reset time
/// and encounters when it is in one.
struct CreatureRespawnRow
{
    uint32 guid = 0;
    uint64 respawntime = 0;
    uint32 map = 0;
    uint32 instance = 0;
    uint8 difficulty = 0;
    uint64 resettime = 0;
    uint32 encountersMask = 0;
};

/// One game object respawn time (`gameobject_respawn`), with its instance's map, difficulty, reset
/// time and encounters when it is in one.
struct GameObjectRespawnRow
{
    uint32 guid = 0;
    uint64 respawntime = 0;
    uint32 map = 0;
    uint32 instance = 0;
    uint8 difficulty = 0;
    uint64 resettime = 0;
    uint32 encountersMask = 0;
};

/// One instance's latest creature respawn time.
struct LatestCreatureRespawnRow
{
    uint64 latestRespawnTime = 0;
    uint32 instance = 0;
};

/// One `instance` row.
struct InstanceIdRow
{
    uint32 id = 0;
};

/// One `instance_reset` row.
struct InstanceResetRow
{
    uint32 mapid = 0;
    uint32 difficulty = 0;
    uint64 resettime = 0;
};

/// One instance with its map, difficulty and reset time (`instance`).
struct InstanceResetTimeRow
{
    uint32 id = 0;
    uint32 map = 0;
    uint32 difficulty = 0;
    uint64 resettime = 0;
};

/// The instances' saved rows (`instance`, `instance_reset`, `creature_respawn`,
/// `gameobject_respawn`, and the binds an instance's delete and renumbering touch). The startup
/// reads, cleanups and reset-time fixes run synchronously, the ones named Now through a direct
/// write; every other write is queued on the character database.
namespace InstanceRepository
{
    /// Writes a spawn's respawn time in one transaction: the old row's DELETE, then, when keep is
    /// set, the new row's INSERT.
    void SaveCreatureRespawn(uint32 guid, uint32 instanceId, uint64 respawnTime, bool keep);
    void SaveGameObjectRespawn(uint32 guid, uint32 instanceId, uint64 respawnTime, bool keep);

    /// Deletes every respawn time of an instance, in one transaction.
    void DeleteRespawnsOfInstance(uint32 instanceId);

    /// Each deletes the respawn times that have passed, synchronously, before the load.
    void DeleteExpiredCreatureRespawnsNow();
    void DeleteExpiredGameObjectRespawnsNow();

    /// Each selects every saved respawn time with its instance: guid, respawn time, map, instance,
    /// difficulty, reset time and the encounters mask.
    std::vector<CreatureRespawnRow> QueryCreatureRespawns();
    std::vector<GameObjectRespawnRow> QueryGameObjectRespawns();

    /// Inserts a new instance's row, the data bound.
    void InsertInstance(uint32 instanceId, uint32 map, uint64 resetTime, uint32 difficulty, uint32 encountersMask, std::string const& data);

    /// Sets an instance's completed encounters.
    void SetEncountersMask(uint32 encountersMask, uint32 instanceId);

    /// Sets an instance's reset time; the Now one writes it directly.
    void SetResetTime(uint64 resetTime, uint32 instanceId);
    void SetResetTimeNow(uint64 resetTime, uint32 instanceId);

    /// Deletes an instance and every bind and respawn time of it, in one transaction.
    void DeleteInstance(uint32 instanceId);

    /// Deletes every instance of a map and their binds, in one transaction.
    void DeleteInstancesOfMap(uint32 map);

    /// Selects every instance with a reset time: id, map, difficulty, reset time.
    std::vector<InstanceResetTimeRow> QueryInstancesWithResetTime();

    /// Selects every instance's latest creature respawn time: the time, the instance.
    std::vector<LatestCreatureRespawnRow> QueryLatestCreatureRespawns();

    /// Selects every instance's id.
    std::vector<InstanceIdRow> QueryInstanceIds();

    /// Gives an instance a new id in every table that names it, in one transaction.
    void RenumberInstance(uint32 newId, uint32 oldId);

    /// Selects every map's global reset time: map, difficulty, reset time.
    std::vector<InstanceResetRow> QueryInstanceResets();

    /// Each writes a map's global reset time; the Now ones write it directly. The two spellings of
    /// the update are both kept.
    void SetInstanceReset(uint64 resetTime, uint32 map, uint32 difficulty);
    void SetInstanceResetNow(uint64 resetTime, uint32 map, uint32 difficulty);
    void SetInstanceResetNowUnquoted(uint64 resetTime, uint32 map, uint32 difficulty);
    void InsertInstanceResetNow(uint32 map, uint32 difficulty, uint64 resetTime);
    void DeleteInstanceResetNow(uint32 map, uint32 difficulty);

    /// The startup cleanup's deletes, each selecting the rows it removes first and deleting them one
    /// by one: binds whose character or group is gone, instances nobody is bound to, binds to a
    /// missing instance, and the respawn times of a missing instance.
    void DeleteCharacterBindsOfMissingCharacters();
    void DeleteGroupBindsOfMissingGroups();
    void DeleteUnboundInstances();
    void DeleteCharacterBindsOfMissingInstances();
    void DeleteGroupBindsOfMissingInstances();
    void DeleteRespawnsOfMissingInstances();

    /// Deletes the instances whose reset time has passed, one by one.
    void DeleteExpiredInstances(uint64 time);

    /// Selects the highest instance id, at start-up.
    std::optional<uint32> QueryHighestId();
}

#endif
