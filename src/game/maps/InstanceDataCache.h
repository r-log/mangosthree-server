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

#ifndef MANGOS_H_INSTANCEDATACACHE_H
#define MANGOS_H_INSTANCEDATACACHE_H

#include "Platform/Define.h"
#include "Policies/Singleton.h"

#include <shared_mutex>
#include <string>
#include <unordered_map>

/**
 * @brief One row's script-data column, as the row answers it.
 *
 * `Map::CreateInstanceData` used to ask MySQL for this on EVERY map creation --
 * continents included -- and the three states it can be in are not the same thing:
 *
 *   no row            the map has never been saved. A continent inserts an empty row
 *                     here (later code expects `world` to have one); an instance does
 *                     nothing.
 *   a row, `data` NULL nothing is loaded. `Field::GetString()` answers NULL and the old
 *                     code skipped `InstanceData::Load`.
 *   a row, `data` set  `InstanceData::Load(value)`, even when the value is empty --
 *                     which is what a freshly inserted `world` row holds.
 *
 * So the cache carries all three, rather than collapsing "NULL" onto "empty".
 */
struct InstanceScriptData
{
    bool        present = false;                            ///< the row exists
    bool        isNull  = false;                            ///< ...and its `data` column is SQL NULL
    std::string value;                                      ///< the column, when it is neither
};

/**
 * @brief The `instance`.`data` and `world`.`data` columns, in memory (decoupling D7i).
 *
 * `Map::CreateInstanceData(true)` ran one synchronous `SELECT` per map created -- every
 * continent at start-up, every transport deck, and every dungeon a player walks into,
 * on the tick. The rows are per-map and per-instance, small, and nothing outside this
 * server writes them while it runs, so they are loaded once and kept current at every
 * writer instead.
 *
 * The D7c invariant applies unchanged: **every writer of those two columns updates this
 * cache where it queues its statement**, so the cache and the table say the same thing
 * from the same instruction. The writers are, in full:
 *
 *   DungeonPersistentState::SaveToDB          INSERT INTO `instance`  -> SetInstance + SetInstanceMap
 *   InstanceData::SaveToDB                    UPDATE `instance`/`world` -> UpdateInstance/UpdateWorld
 *   Map::CreateInstanceData                   INSERT INTO `world`     -> SetWorld("")
 *   MapPersistentStateManager::DeleteInstanceFromDB
 *                                             DELETE FROM `instance`  -> RemoveInstance
 *   MapPersistentStateManager::_ResetOrWarnAll
 *                                             DELETE FROM `instance` WHERE `map` -> RemoveInstancesOfMap
 *
 * `CleanupInstances()` and `PackInstances()` also delete and renumber `instance` rows,
 * and are deliberately NOT mirrored: both run in World::SetInitialWorldSettings BEFORE
 * LoadFromDB(), so the cache is built from what they left behind. The `resettime` and
 * `encountersMask` updates are not mirrored either -- this cache holds `data` and
 * nothing else.
 *
 * Threading: map creation takes MapManager's lock on whichever thread asked for the map,
 * and `InstanceData::SaveToDB` runs on a map-update worker, so readers and writers are on
 * different threads. One `std::shared_mutex`, readers shared and writers exclusive -- the
 * same discipline as CharacterCache (D7c).
 *
 * @note Like every other table the server loads at start-up, a row edited from OUTSIDE
 * the server while it runs is invisible until the next restart.
 */
class InstanceDataCache : public MaNGOS::Singleton<InstanceDataCache>
{
        friend class MaNGOS::Singleton<InstanceDataCache>;

    public:

        /// The two start-up reads: `instance` and `world`. Called from
        /// World::SetInitialWorldSettings, after CleanupInstances()/PackInstances() have
        /// had their way with the table and before any map can be created.
        void LoadFromDB();

        // ---- read side -------------------------------------------------------------

        /// The `instance` row for an instance id, as Map::CreateInstanceData asked for it.
        InstanceScriptData GetInstance(uint32 instanceId) const;

        /// The `world` row for a (non-instanceable) map id.
        InstanceScriptData GetWorld(uint32 mapId) const;

        size_t InstanceCount() const;
        size_t WorldCount() const;

        // ---- write side, one method per writer -------------------------------------

        /// An `instance` row was INSERTed with this `data` (creates the entry).
        void SetInstance(uint32 instanceId, std::string const& data);

        /// A `world` row was INSERTed with this `data` (creates the entry).
        void SetWorld(uint32 mapId, std::string const& data);

        /// `UPDATE instance SET data = ? WHERE id = ?`. Like the statement, it changes an
        /// existing row only: a missing row stays missing (fix round 1).
        void UpdateInstance(uint32 instanceId, std::string const& data);

        /// `UPDATE world SET data = ? WHERE map = ?`, likewise existing rows only.
        void UpdateWorld(uint32 mapId, std::string const& data);

        /// `DELETE FROM instance WHERE id = <instanceId>`.
        void RemoveInstance(uint32 instanceId);

        /// `DELETE FROM instance WHERE map = <mapId>` -- the global raid reset.
        void RemoveInstancesOfMap(uint32 mapId);

        /// The map id an instance id belongs to (`instance`.`map`), so RemoveInstancesOfMap
        /// can find its rows without going back to the table. Written by LoadFromDB and by
        /// DungeonPersistentState::SaveToDB beside its INSERT -- production, not a seam.
        void SetInstanceMap(uint32 instanceId, uint32 mapId);

        /// Everything back to empty. The tests, and nothing else.
        void Clear();

    private:

        struct CachedRow
        {
            bool        isNull = false;
            std::string value;
            uint32      mapId = 0;                          ///< `instance`.`map`, 0 for `world` rows
        };

        mutable std::shared_mutex m_lock;
        std::unordered_map<uint32, CachedRow> m_instances;  ///< by `instance`.`id`
        std::unordered_map<uint32, CachedRow> m_worlds;     ///< by `world`.`map`
};

#define sInstanceDataCache MaNGOS::Singleton<InstanceDataCache>::Instance()

#endif
