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

/**
 * @file PoolSpawner.cpp
 * @brief The pool system's runtime: the spawns of each pool in each map state
 *
 * PoolSpawner rolls the members of a pool, spawns and despawns them in a map's
 * persistent state and respawns a pool when one of its members is ready to
 * respawn. It reads the pools PoolManager loaded.
 */

#include "Utilities/Errors.h"
#include "PoolSpawner.h"
#include "Creature.h"
#include "GameObject.h"
#include "Log.h"
#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "data/GameConfig.h"

////////////////////////////////////////////////////////////
// template class SpawnedPoolData

// Method that tell amount spawned objects/subpools
uint32 SpawnedPoolData::GetSpawnedObjects(uint32 pool_id) const
{
    SpawnedPoolPools::const_iterator itr = mSpawnedPools.find(pool_id);
    return itr != mSpawnedPools.end() ? itr->second : 0;
}

// Method that tell if a creature is spawned currently
template<>

/**
 * @brief Checks whether a creature spawn is currently active in a pool state.
 *
 * @param db_guid The creature spawn guid.
 * @return true if the creature is marked as spawned; otherwise false.
 */
bool SpawnedPoolData::IsSpawnedObject<Creature>(uint32 db_guid) const
{
    return mSpawnedCreatures.find(db_guid) != mSpawnedCreatures.end();
}

// Method that tell if a gameobject is spawned currently
template<>

/**
 * @brief Checks whether a gameobject spawn is currently active in a pool state.
 *
 * @param db_guid The gameobject spawn guid.
 * @return true if the gameobject is marked as spawned; otherwise false.
 */
bool SpawnedPoolData::IsSpawnedObject<GameObject>(uint32 db_guid) const
{
    return mSpawnedGameobjects.find(db_guid) != mSpawnedGameobjects.end();
}

// Method that tell if a pool is spawned currently
template<>

/**
 * @brief Checks whether a child pool is currently active in a pool state.
 *
 * @param sub_pool_id The child pool id.
 * @return true if the child pool is marked as spawned; otherwise false.
 */
bool SpawnedPoolData::IsSpawnedObject<Pool>(uint32 sub_pool_id) const
{
    return mSpawnedPools.find(sub_pool_id) != mSpawnedPools.end();
}

template<>

/**
 * @brief Marks a creature spawn as active in a pool.
 *
 * @param db_guid The creature spawn guid.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::AddSpawn<Creature>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedCreatures.insert(db_guid);
    ++mSpawnedPools[pool_id];
}

template<>

/**
 * @brief Marks a gameobject spawn as active in a pool.
 *
 * @param db_guid The gameobject spawn guid.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::AddSpawn<GameObject>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedGameobjects.insert(db_guid);
    ++mSpawnedPools[pool_id];
}

template<>

/**
 * @brief Marks a child pool as active in a parent pool.
 *
 * @param sub_pool_id The child pool id.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::AddSpawn<Pool>(uint32 sub_pool_id, uint32 pool_id)
{
    mSpawnedPools[sub_pool_id] = 0;
    ++mSpawnedPools[pool_id];
}

template<>

/**
 * @brief Removes an active creature spawn from a pool.
 *
 * @param db_guid The creature spawn guid.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::RemoveSpawn<Creature>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedCreatures.erase(db_guid);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
    {
        --val;
    }
}

template<>

/**
 * @brief Removes an active gameobject spawn from a pool.
 *
 * @param db_guid The gameobject spawn guid.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::RemoveSpawn<GameObject>(uint32 db_guid, uint32 pool_id)
{
    mSpawnedGameobjects.erase(db_guid);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
    {
        --val;
    }
}

template<>

/**
 * @brief Removes an active child pool from a parent pool.
 *
 * @param sub_pool_id The child pool id.
 * @param pool_id The parent pool id.
 */
void SpawnedPoolData::RemoveSpawn<Pool>(uint32 sub_pool_id, uint32 pool_id)
{
    mSpawnedPools.erase(sub_pool_id);
    uint32& val = mSpawnedPools[pool_id];
    if (val > 0)
    {
        --val;
    }
}

template <class T>

/**
 * @brief Chooses one spawn candidate from the pool group.
 *
 * @param spawns The spawned state tracker.
 * @param triggerFrom The guid that triggered the respawn cycle.
 * @return The selected pool object, or null if none can be chosen.
 */
PoolObject const* PoolSpawner::RollOne(PoolGroup<T> const& group, SpawnedPoolData& spawns, uint32 triggerFrom)
{
    PoolObjectList const& explicitlyChanced = group.GetExplicitlyChanced();
    PoolObjectList const& equalChanced = group.GetEqualChanced();

    if (!explicitlyChanced.empty())
    {
        float roll = (float)rand_chance();

        for (uint32 i = 0; i < explicitlyChanced.size(); ++i)
        {
            roll -= explicitlyChanced[i].chance;
            // Triggering object is marked as spawned at this time and can be also rolled (respawn case)
            // so this need explicit check for this case
            if (roll < 0 && !explicitlyChanced[i].exclude && (explicitlyChanced[i].guid == triggerFrom || !spawns.IsSpawnedObject<T>(explicitlyChanced[i].guid)))
            {
                return &explicitlyChanced[i];
            }
        }
    }

    if (!equalChanced.empty())
    {
        int32 index = irand(0, equalChanced.size() - 1);
        // Triggering object is marked as spawned at this time and can be also rolled (respawn case)
        // so this need explicit check for this case
        if (!equalChanced[index].exclude && (equalChanced[index].guid == triggerFrom || !spawns.IsSpawnedObject<T>(equalChanced[index].guid)))
        {
            return &equalChanced[index];
        }
    }

    return NULL;
}

// Main method to despawn a creature or gameobject in a pool
// If no guid is passed, the pool is just removed (event end case)
// If guid is filled, cache will be used and no removal will occur, it just fill the cache
template<class T>

/**
 * @brief Despawns pooled objects of a given type.
 *
 * @param mapState The persistent state owning the spawns.
 * @param guid Optional specific guid to despawn.
 */
void PoolSpawner::DespawnObject(PoolGroup<T> const& group, MapPersistentState& mapState, uint32 guid)
{
    PoolObjectList const& explicitlyChanced = group.GetExplicitlyChanced();
    PoolObjectList const& equalChanced = group.GetEqualChanced();

    for (size_t i = 0; i < equalChanced.size(); ++i)
    {
        // if spawned
        if (mapState.GetSpawnedPoolData().IsSpawnedObject<T>(equalChanced[i].guid))
        {
            // any or specially requested
            if (!guid || equalChanced[i].guid == guid)
            {
                Despawn1Object<T>(mapState, equalChanced[i].guid);
                mapState.GetSpawnedPoolData().RemoveSpawn<T>(equalChanced[i].guid, group.GetPoolId());
            }
        }
    }

    for (size_t i = 0; i < explicitlyChanced.size(); ++i)
    {
        // spawned
        if (mapState.GetSpawnedPoolData().IsSpawnedObject<T>(explicitlyChanced[i].guid))
        {
            // any or specially requested
            if (!guid || explicitlyChanced[i].guid == guid)
            {
                Despawn1Object<T>(mapState, explicitlyChanced[i].guid);
                mapState.GetSpawnedPoolData().RemoveSpawn<T>(explicitlyChanced[i].guid, group.GetPoolId());
            }
        }
    }
}

// Method that is actualy doing the removal job on one creature
template<>

/**
 * @brief Despawns one pooled creature.
 *
 * @param mapState The persistent state owning the spawn.
 * @param guid The creature spawn guid.
 */
void PoolSpawner::Despawn1Object<Creature>(MapPersistentState& mapState, uint32 guid)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->RemoveCreatureFromGrid(guid, data);

            if (Map* dataMap = dataMapState->GetMap())
                if (Creature* pCreature = dataMap->GetCreature(data->GetObjectGuid(guid)))
                {
                    pCreature->AddObjectToRemoveList();
                }
        }
    }
}

// Same on one gameobject
template<>

/**
 * @brief Despawns one pooled gameobject.
 *
 * @param mapState The persistent state owning the spawn.
 * @param guid The gameobject spawn guid.
 */
void PoolSpawner::Despawn1Object<GameObject>(MapPersistentState& mapState, uint32 guid)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->RemoveGameobjectFromGrid(guid, data);

            if (Map* dataMap = dataMapState->GetMap())
                if (GameObject* pGameobject = dataMap->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, guid)))
                {
                    pGameobject->AddObjectToRemoveList();
                }
        }
    }
}

// Same on one pool
template<>

/**
 * @brief Despawns one pooled child pool.
 *
 * @param mapState The persistent state owning the pool.
 * @param child_pool_id The child pool id.
 */
void PoolSpawner::Despawn1Object<Pool>(MapPersistentState& mapState, uint32 child_pool_id)
{
    DespawnPool(mapState, child_pool_id);
}

template <class T>

/**
 * @brief Spawns objects from the pool group up to its configured limit.
 *
 * @param mapState The persistent state receiving the spawns.
 * @param limit The desired spawn limit.
 * @param triggerFrom The guid that triggered the respawn cycle.
 * @param instantly Whether spawns should bypass fresh respawn delays.
 */
void PoolSpawner::SpawnObject(PoolGroup<T> const& group, MapPersistentState& mapState, uint32 limit, uint32 triggerFrom, bool instantly)
{
    SpawnedPoolData& spawns = mapState.GetSpawnedPoolData();

    uint32 lastDespawned = 0;
    int count = limit - spawns.GetSpawnedObjects(group.GetPoolId());

    // If triggered from some object respawn this object is still marked as spawned
    // and also counted into m_SpawnedPoolAmount so we need increase count to be
    // spawned by 1
    if (triggerFrom)
    {
        if (spawns.IsSpawnedObject<T>(triggerFrom))
        {
            ++count;
        }
        else
        {
            triggerFrom = 0;
        }
    }

    // This will try to spawn the rest of pool, not guaranteed
    for (int i = 0; i < count; ++i)
    {
        PoolObject const* obj = RollOne(group, spawns, triggerFrom);
        if (!obj)
        {
            continue;
        }
        if (obj->guid == lastDespawned)
        {
            continue;
        }

        if (obj->guid == triggerFrom)
        {
            MANGOS_ASSERT(spawns.IsSpawnedObject<T>(obj->guid));
            MANGOS_ASSERT(spawns.GetSpawnedObjects(group.GetPoolId()) > 0);
            ReSpawn1Object<T>(mapState, obj);
            triggerFrom = 0;
            continue;
        }

        spawns.AddSpawn<T>(obj->guid, group.GetPoolId());
        Spawn1Object<T>(mapState, obj, instantly);

        if (triggerFrom)
        {
            // One spawn one despawn no count increase
            DespawnObject(group, mapState, triggerFrom);
            lastDespawned = triggerFrom;
            triggerFrom = 0;
        }
    }
}

// Method that is actualy doing the spawn job on 1 creature
template <>

/**
 * @brief Spawns one pooled creature.
 *
 * @param mapState The persistent state receiving the spawn.
 * @param obj The pooled object entry.
 * @param instantly Whether to spawn immediately without a fresh delay.
 */
void PoolSpawner::Spawn1Object<Creature>(MapPersistentState& mapState, PoolObject const* obj, bool instantly)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->AddCreatureToGrid(obj->guid, data);

            Map* dataMap = dataMapState->GetMap();

            // We use spawn coords to spawn
            if (dataMap && dataMap->IsCellLoaded(data->posX, data->posY))
            {
                Creature* pCreature = new Creature;
                // DEBUG_LOG("Spawning creature %u",obj->guid);
                if (!pCreature->LoadFromDB(obj->guid, dataMap))
                {
                    delete pCreature;
                    return;
                }
                else
                {
                    // if new spawn replaces a just despawned creature, not instantly spawn but set respawn timer
                    if (!instantly)
                    {
                        pCreature->SetRespawnTime(pCreature->GetRespawnDelay());
                        if (sGameConfig.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY) || pCreature->IsWorldBoss())
                        {
                            pCreature->SaveRespawnTime();
                        }
                    }
                    dataMap->Add(pCreature);
                }
            }
            // for not loaded grid just update respawn time (avoid work for instances until implemented support)
            else if (!instantly)
            {
                dataMapState->SaveCreatureRespawnTime(obj->guid, time(NULL) + data->spawntimesecs);
            }
        }
    }
}

// Same for 1 gameobject
template <>

/**
 * @brief Spawns one pooled gameobject.
 *
 * @param mapState The persistent state receiving the spawn.
 * @param obj The pooled object entry.
 * @param instantly Whether to spawn immediately without a fresh delay.
 */
void PoolSpawner::Spawn1Object<GameObject>(MapPersistentState& mapState, PoolObject const* obj, bool instantly)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
        {
            dataMapState->AddGameobjectToGrid(obj->guid, data);

            Map* dataMap = dataMapState->GetMap();

            // We use spawn coords to spawn
            if (dataMap && dataMap->IsCellLoaded(data->posX, data->posY))
            {
                GameObject* pGameobject = new GameObject;
                // DEBUG_LOG("Spawning gameobject %u", obj->guid);
                if (!pGameobject->LoadFromDB(obj->guid, dataMap))
                {
                    delete pGameobject;
                    return;
                }
                else
                {
                    if (pGameobject->isSpawnedByDefault())
                    {
                        // if new spawn replaces a just despawned object, not instantly spawn but set respawn timer
                        if (!instantly)
                        {
                            pGameobject->SetRespawnTime(pGameobject->GetRespawnDelay());
                            if (sGameConfig.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
                            {
                                pGameobject->SaveRespawnTime();
                            }
                        }
                        dataMap->Add(pGameobject);
                    }
                }
            }
            // for not loaded grid just update respawn time (avoid work for instances until implemented support)
            else if (!instantly)
            {
                // for spawned by default object only
                if (data->spawntimesecs >= 0)
                {
                    dataMapState->SaveGORespawnTime(obj->guid, time(NULL) + data->spawntimesecs);
                }
            }
        }
    }
}

// Same for 1 pool
template <>

/**
 * @brief Spawns one pooled child pool.
 *
 * @param mapState The persistent state receiving the spawn.
 * @param obj The pooled child-pool entry.
 * @param instantly Whether leaf spawns should appear immediately.
 */
void PoolSpawner::Spawn1Object<Pool>(MapPersistentState& mapState, PoolObject const* obj, bool instantly)
{
    SpawnPool(mapState, obj->guid, instantly);
}

// Method that does the respawn job on the specified creature
template <>

/**
 * @brief Re-adds an already selected pooled creature to the live map.
 *
 * @param mapState The persistent state owning the spawn.
 * @param obj The pooled object entry.
 */
void PoolSpawner::ReSpawn1Object<Creature>(MapPersistentState& mapState, PoolObject const* obj)
{
    if (CreatureData const* data = sObjectMgr.GetCreatureData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
            if (Map* dataMap = dataMapState->GetMap())
                if (Creature* pCreature = dataMap->GetCreature(data->GetObjectGuid(obj->guid)))
                {
                    pCreature->GetMap()->Add(pCreature);
                }
    }
}

// Method that does the respawn job on the specified gameobject
template <>

/**
 * @brief Re-adds an already selected pooled gameobject to the live map.
 *
 * @param mapState The persistent state owning the spawn.
 * @param obj The pooled object entry.
 */
void PoolSpawner::ReSpawn1Object<GameObject>(MapPersistentState& mapState, PoolObject const* obj)
{
    if (GameObjectData const* data = sObjectMgr.GetGOData(obj->guid))
    {
        // for non-instanceable maps pool spawn can be at different map from provided mapState
        if (MapPersistentState* dataMapState = mapState.GetMapId() == data->mapid ? &mapState : sMapPersistentStateMgr.GetPersistentState(data->mapid, 0))
            if (Map* dataMap = dataMapState->GetMap())
                if (GameObject* pGameobject = dataMap->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, obj->guid)))
                {
                    pGameobject->GetMap()->Add(pGameobject);
                }
    }
}

// Nothing to do for a child Pool
template <>

/**
 * @brief No-op respawn handler for child pools.
 *
 * @param mapState Unused persistent state.
 * @param obj Unused pool object.
 */
void PoolSpawner::ReSpawn1Object<Pool>(MapPersistentState& /*mapState*/, PoolObject const* /*obj*/)
{
}

// The initialize method will spawn all pools not in an event and not in another pool
void PoolSpawner::Initialize(MapPersistentState* state)
{
    // spawn pools for expected map or for not initialized shared pools state for non-instanceable maps
    for (uint16 pool_entry = 0; pool_entry < sPoolMgr.GetPoolTemplates().size(); ++pool_entry)
        if (sPoolMgr.GetPoolTemplate(pool_entry).AutoSpawn)
        {
            InitSpawnPool(*state, pool_entry);
        }
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the creature is respawned only (added back to map)
template<>

/**
 * @brief Spawns a creature pool group.
 *
 * @param mapState The persistent state receiving the spawns.
 * @param pool_id The pool id.
 * @param db_guid The triggering spawn guid.
 * @param instantly Whether to spawn immediately.
 */
void PoolSpawner::SpawnPoolGroup<Creature>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid, bool instantly)
{
    if (!sPoolMgr.GetPoolCreatures(pool_id).isEmpty())
    {
        SpawnObject(sPoolMgr.GetPoolCreatures(pool_id), mapState, sPoolMgr.GetPoolTemplate(pool_id).MaxLimit, db_guid, instantly);
    }
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the gameobject is respawned only (added back to map)
template<>

/**
 * @brief Spawns a gameobject pool group.
 *
 * @param mapState The persistent state receiving the spawns.
 * @param pool_id The pool id.
 * @param db_guid The triggering spawn guid.
 * @param instantly Whether to spawn immediately.
 */
void PoolSpawner::SpawnPoolGroup<GameObject>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid, bool instantly)
{
    if (!sPoolMgr.GetPoolGameObjects(pool_id).isEmpty())
    {
        SpawnObject(sPoolMgr.GetPoolGameObjects(pool_id), mapState, sPoolMgr.GetPoolTemplate(pool_id).MaxLimit, db_guid, instantly);
    }
}

// Call to spawn a pool, if cache if true the method will spawn only if cached entry is different
// If it's same, the pool is respawned only
template<>

/**
 * @brief Spawns a child-pool group.
 *
 * @param mapState The persistent state receiving the spawns.
 * @param pool_id The pool id.
 * @param sub_pool_id The triggering child pool id.
 * @param instantly Whether to spawn immediately.
 */
void PoolSpawner::SpawnPoolGroup<Pool>(MapPersistentState& mapState, uint16 pool_id, uint32 sub_pool_id, bool instantly)
{
    if (!sPoolMgr.GetPoolPools(pool_id).isEmpty())
    {
        SpawnObject(sPoolMgr.GetPoolPools(pool_id), mapState, sPoolMgr.GetPoolTemplate(pool_id).MaxLimit, sub_pool_id, instantly);
    }
}

/*!
    \param instantly defines if (leaf-)objects are spawned instantly or with fresh respawn timer */
void PoolSpawner::SpawnPool(MapPersistentState& mapState, uint16 pool_id, bool instantly)
{
    SpawnPoolGroup<Pool>(mapState, pool_id, 0, instantly);
    SpawnPoolGroup<GameObject>(mapState, pool_id, 0, instantly);
    SpawnPoolGroup<Creature>(mapState, pool_id, 0, instantly);
}

// Call to despawn a pool, all gameobjects/creatures in this pool are removed
void PoolSpawner::DespawnPool(MapPersistentState& mapState, uint16 pool_id)
{
    if (!sPoolMgr.GetPoolCreatures(pool_id).isEmpty())
    {
        DespawnObject(sPoolMgr.GetPoolCreatures(pool_id), mapState);
    }

    if (!sPoolMgr.GetPoolGameObjects(pool_id).isEmpty())
    {
        DespawnObject(sPoolMgr.GetPoolGameObjects(pool_id), mapState);
    }

    if (!sPoolMgr.GetPoolPools(pool_id).isEmpty())
    {
        DespawnObject(sPoolMgr.GetPoolPools(pool_id), mapState);
    }
}

// Call to update the pool when a gameobject/creature part of pool [pool_id] is ready to respawn
// Here we cache only the creature/gameobject whose guid is passed as parameter
// Then the spawn pool call will use this cache to decide
template<typename T>

/**
 * @brief Updates a pool after one of its members or child pools changes state.
 *
 * @param mapState The persistent state containing the pool.
 * @param pool_id The pool id.
 * @param db_guid_or_pool_id The triggering member guid or child pool id.
 */
void PoolSpawner::UpdatePool(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id)
{
    if (uint16 motherpoolid = sPoolMgr.IsPartOfAPool<Pool>(pool_id))
    {
        SpawnPoolGroup<Pool>(mapState, motherpoolid, pool_id, false);
    }
    else
    {
        SpawnPoolGroup<T>(mapState, pool_id, db_guid_or_pool_id, false);
    }
}

template void PoolSpawner::UpdatePool<Pool>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolSpawner::UpdatePool<GameObject>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolSpawner::UpdatePool<Creature>(MapPersistentState& mapState, uint16 pool_id, uint32 db_guid_or_pool_id);

struct SpawnPoolInMapsWorker
{
    explicit SpawnPoolInMapsWorker(PoolSpawner& mgr, uint32 pool_id, bool instantly)
        : i_mgr(mgr), i_pool_id(pool_id), i_instantly(instantly) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.SpawnPool(*state, i_pool_id, i_instantly);
    }

    PoolSpawner& i_mgr;
    uint32 i_pool_id;
    bool i_instantly;
};

// used for calling from global systems when need spawn pool in all appropriate map persistent states
void PoolSpawner::SpawnPoolInMaps(uint16 pool_id, bool instantly)
{
    PoolTemplateData const& poolTemplate = sPoolMgr.GetPoolTemplate(pool_id);

    // pool no have spawns (base at loading algo
    if (!poolTemplate.mapEntry)
    {
        return;
    }

    SpawnPoolInMapsWorker worker(*this, pool_id, instantly);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->ID, worker);
}

struct DespawnPoolInMapsWorker
{
    explicit DespawnPoolInMapsWorker(PoolSpawner& mgr, uint32 pool_id)
        : i_mgr(mgr), i_pool_id(pool_id) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.DespawnPool(*state, i_pool_id);
    }

    PoolSpawner& i_mgr;
    uint32 i_pool_id;
};

// used for calling from global systems when need spawn pool in all appropriate map persistent states
void PoolSpawner::DespawnPoolInMaps(uint16 pool_id)
{
    PoolTemplateData const& poolTemplate = sPoolMgr.GetPoolTemplate(pool_id);

    // pool no have spawns (base at loading algo
    if (!poolTemplate.mapEntry)
    {
        return;
    }

    DespawnPoolInMapsWorker worker(*this, pool_id);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->ID, worker);
}

/**
 * @brief Spawns a pool in a persistent state if the map is compatible.
 *
 * @param mapState The target persistent state.
 * @param pool_id The pool id.
 */
void PoolSpawner::InitSpawnPool(MapPersistentState& mapState, uint16 pool_id)
{
    // spawn pool for expected map or for not initialized shared pools state for non-instanceable maps
    if (sPoolMgr.GetPoolTemplate(pool_id).CanBeSpawnedAtMap(mapState.GetMapEntry()))
    {
        SpawnPool(mapState, pool_id, true);
    }
}

template<typename T>
struct UpdatePoolInMapsWorker
{
    explicit UpdatePoolInMapsWorker(PoolSpawner& mgr, uint32 pool_id, uint32 db_guid_or_pool_id)
        : i_mgr(mgr), i_pool_id(pool_id), i_db_guid_or_pool_id(db_guid_or_pool_id) {}

    void operator()(MapPersistentState* state)
    {
        i_mgr.UpdatePool<T>(*state, i_pool_id, i_db_guid_or_pool_id);
    }

    PoolSpawner& i_mgr;
    uint32 i_pool_id;
    uint32 i_db_guid_or_pool_id;
};

template<typename T>

/**
 * @brief Updates a pool across all matching persistent states.
 *
 * @param pool_id The pool id.
 * @param db_guid_or_pool_id The triggering member guid or child pool id.
 */
void PoolSpawner::UpdatePoolInMaps(uint16 pool_id, uint32 db_guid_or_pool_id)
{
    PoolTemplateData const& poolTemplate = sPoolMgr.GetPoolTemplate(pool_id);

    // pool no have spawns (base at loading algo
    if (!poolTemplate.mapEntry)
    {
        return;
    }

    UpdatePoolInMapsWorker<T> worker(*this, pool_id, db_guid_or_pool_id);
    sMapPersistentStateMgr.DoForAllStatesWithMapId(poolTemplate.mapEntry->ID, worker);
}

template void PoolSpawner::UpdatePoolInMaps<Pool>(uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolSpawner::UpdatePoolInMaps<GameObject>(uint16 pool_id, uint32 db_guid_or_pool_id);
template void PoolSpawner::UpdatePoolInMaps<Creature>(uint16 pool_id, uint32 db_guid_or_pool_id);
