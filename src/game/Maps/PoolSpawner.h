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

#ifndef MANGOS_H_POOLSPAWNER
#define MANGOS_H_POOLSPAWNER

#include <map>
#include <set>
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "PoolManager.h"

class MapPersistentState;

typedef std::set<uint32> SpawnedPoolObjects;
typedef std::map<uint32, uint32> SpawnedPoolPools;

class SpawnedPoolData
{
    public:
        SpawnedPoolData() : m_isInitialized(false) {}

        template<typename T>
        bool IsSpawnedObject(uint32 db_guid_or_pool_id) const;

        uint32 GetSpawnedObjects(uint32 pool_id) const;

        template<typename T>
        void AddSpawn(uint32 db_guid_or_pool_id, uint32 pool_id);

        template<typename T>
        void RemoveSpawn(uint32 db_guid_or_pool_id, uint32 pool_id);

        bool IsInitialized() const { return m_isInitialized; }
        void SetInitialized() { m_isInitialized = true; }

        SpawnedPoolObjects const& GetSpawnedCreatures() const { return mSpawnedCreatures; }
        SpawnedPoolObjects const& GetSpawnedGameobjects() const { return mSpawnedGameobjects; }
        SpawnedPoolPools const& GetSpawnedPools() const { return mSpawnedPools; }
    private:
        SpawnedPoolObjects mSpawnedCreatures;
        SpawnedPoolObjects mSpawnedGameobjects;
        SpawnedPoolPools   mSpawnedPools;
        bool m_isInitialized;
};

/// The pool system's runtime: rolls a pool's members and spawns or despawns them in a map's persistent
/// state, over the pools PoolManager loaded.
class PoolSpawner
{
    public:
        void Initialize(MapPersistentState* state);         // called at new MapPersistentState object create

        void SpawnPool(MapPersistentState& mapState, uint16 pool_id, bool instantly);
        void DespawnPool(MapPersistentState& mapState, uint16 pool_id);

        template<typename T>
        void UpdatePool(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id = 0);

        // used for calling from global systems when need spawn pool in all appropriate map instances
        void SpawnPoolInMaps(uint16 pool_id, bool instantly);
        void DespawnPoolInMaps(uint16 pool_id);

        // used for calling from global systems when need initialize spawn pool state in appropriate (possible) map persistent state
        void InitSpawnPool(MapPersistentState& mapState, uint16 pool_id);

        template<typename T>
        void UpdatePoolInMaps(uint16 pool_id, uint32 db_guid_or_pool_id = 0);

    private:
        template<typename T>
        void SpawnPoolGroup(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id, bool instantly);

        template<typename T>
        PoolObject const* RollOne(PoolGroup<T> const& group, SpawnedPoolData& spawns, uint32 triggerFrom);
        template<typename T>
        void SpawnObject(PoolGroup<T> const& group, MapPersistentState& mapState, uint32 limit, uint32 triggerFrom, bool instantly);
        template<typename T>
        void DespawnObject(PoolGroup<T> const& group, MapPersistentState& mapState, uint32 guid = 0);
        template<typename T>
        void Spawn1Object(MapPersistentState& mapState, PoolObject const* obj, bool instantly);
        template<typename T>
        void Despawn1Object(MapPersistentState& mapState, uint32 guid);
        template<typename T>
        void ReSpawn1Object(MapPersistentState& mapState, PoolObject const* obj);
};

#define sPoolSpawner MaNGOS::Singleton<PoolSpawner>::Instance()

#endif
