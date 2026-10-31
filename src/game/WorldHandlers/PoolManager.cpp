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
 * @file PoolManager.cpp
 * @brief Pool system for randomized object spawning
 *
 * This file implements PoolManager, which loads the spawn pools of
 * creatures, game objects and other pools and answers which pool a spawn
 * belongs to; PoolSpawner (PoolSpawner.cpp) spawns them. Features:
 *
 * - Random selection from pool of candidates
 * - Pool limits (max spawned at once)
 * - Hierarchical pools (pools within pools)
 * - Spawn chance weighting
 * - Pool respawn management
 * - Instance-specific pool states
 *
 * Pools allow for dynamic content where only a subset of possible
 * spawns appear at any given time, adding variety to the world.
 *
 * @see PoolManager for the manager class
 * @see PoolSpawner for the spawning and SpawnedPoolData for its state
 */

#include "Utilities/Errors.h"
#include <sstream>
#include <map>
#include <set>
#include "PoolManager.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Log.h"
#include "DBCStores.h"
#include "Policies/Singleton.h"
#include "persistence/world/pools/PoolDataRepository.h"

////////////////////////////////////////////////////////////
// Methods of class PoolObject
template<>

/**
 * @brief Verifies creature event linkage for a pooled object.
 *
 * @param poolId The owning pool id.
 * @param event_id The expected event id.
 * @param creature2event The creature-to-event mapping.
 * @param go2event Unused gameobject-to-event mapping.
 */
void PoolObject::CheckEventLinkAndReport<Creature>(uint32 poolId, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& /*go2event*/) const
{
    std::map<uint32, int16>::const_iterator itr = creature2event.find(guid);
    if (itr == creature2event.end() || itr->second != event_id)
        sLog.outErrorDb("Creature (GUID: %u) expected to be listed in `game_event_creature` for event %u as part pool %u", guid, event_id, poolId);
}

template<>

/**
 * @brief Verifies gameobject event linkage for a pooled object.
 *
 * @param poolId The owning pool id.
 * @param event_id The expected event id.
 * @param creature2event Unused creature-to-event mapping.
 * @param go2event The gameobject-to-event mapping.
 */
void PoolObject::CheckEventLinkAndReport<GameObject>(uint32 poolId, int16 event_id, std::map<uint32, int16> const& /*creature2event*/, std::map<uint32, int16> const& go2event) const
{
    std::map<uint32, int16>::const_iterator itr = go2event.find(guid);
    if (itr == go2event.end() || itr->second != event_id)
        sLog.outErrorDb("Gameobject (GUID: %u) expected to be listed in `game_event_gameobject` for event %u as part pool %u", guid, event_id, poolId);
}

template<>

/**
 * @brief Verifies event linkage for a child pool reference.
 *
 * @param poolId Unused parent pool id.
 * @param event_id The expected event id.
 * @param creature2event The creature-to-event mapping.
 * @param go2event The gameobject-to-event mapping.
 */
void PoolObject::CheckEventLinkAndReport<Pool>(uint32 poolId, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    sPoolMgr.CheckEventLinkAndReport(guid, event_id, creature2event, go2event);
}

////////////////////////////////////////////////////////////
// Methods of template class PoolGroup

// Method to add a gameobject/creature guid to the proper list depending on pool type and chance value
template <class T>

/**
 * @brief Adds an entry to a pool group's equal-chance or explicit-chance list.
 *
 * @param poolitem The pool object entry.
 * @param maxentries The maximum simultaneous entries for the pool.
 */
void PoolGroup<T>::AddEntry(PoolObject& poolitem, uint32 maxentries)
{
    if (poolitem.chance != 0 && maxentries == 1)
    {
        ExplicitlyChanced.push_back(poolitem);
    }
    else
    {
        EqualChanced.push_back(poolitem);
    }
}

// Method to check the chances are proper in this object pool
template <class T>

/**
 * @brief Validates the configured probabilities for a pool group.
 *
 * @return true if the pool definition is valid; otherwise false.
 */
bool PoolGroup<T>::CheckPool() const
{
    if (EqualChanced.empty())
    {
        float chance = 0;
        for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
        {
            chance += ExplicitlyChanced[i].chance;
        }
        if (chance != 100 && chance != 0)
        {
            return false;
        }
    }
    return true;
}

// Method to check event linking
template <class T>

/**
 * @brief Verifies event linkage for every entry in a pool group.
 *
 * @param event_id The expected event id.
 * @param creature2event The creature-to-event mapping.
 * @param go2event The gameobject-to-event mapping.
 */
void PoolGroup<T>::CheckEventLinkAndReport(int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    for (uint32 i = 0; i < EqualChanced.size(); ++i)
    {
        EqualChanced[i].template CheckEventLinkAndReport<T>(poolId, event_id, creature2event, go2event);
    }

    for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
    {
        ExplicitlyChanced[i].template CheckEventLinkAndReport<T>(poolId, event_id, creature2event, go2event);
    }
}

template <class T>

/**
 * @brief Marks an object in the pool group as excluded or included.
 *
 * @param guid The pooled object guid.
 * @param state The exclusion state.
 */
void PoolGroup<T>::SetExcludeObject(uint32 guid, bool state)
{
    for (uint32 i = 0; i < EqualChanced.size(); ++i)
    {
        if (EqualChanced[i].guid == guid)
        {
            EqualChanced[i].exclude = state;
            return;
        }
    }

    for (uint32 i = 0; i < ExplicitlyChanced.size(); ++i)
    {
        if (ExplicitlyChanced[i].guid == guid)
        {
            ExplicitlyChanced[i].exclude = state;
            return;
        }
    }
}

// Method for a pool only to remove any found record causing a circular dependency loop
template<>

/**
 * @brief Removes one child-pool relation from a pool group.
 *
 * @param child_pool_id The child pool id to remove.
 */
void PoolGroup<Pool>::RemoveOneRelation(uint16 child_pool_id)
{
    for (PoolObjectList::iterator itr = ExplicitlyChanced.begin(); itr != ExplicitlyChanced.end(); ++itr)
    {
        if (itr->guid == child_pool_id)
        {
            ExplicitlyChanced.erase(itr);
            break;
        }
    }
    for (PoolObjectList::iterator itr = EqualChanced.begin(); itr != EqualChanced.end(); ++itr)
    {
        if (itr->guid == child_pool_id)
        {
            EqualChanced.erase(itr);
            break;
        }
    }
}

////////////////////////////////////////////////////////////
// Methods of class PoolManager

PoolManager::PoolManager(): max_pool_id(0)
{
}

// Check listing all pool spawns in single instanceable map or only in non-instanceable maps
// This applied to all pools have common mother pool
struct PoolMapChecker
{
    PoolManager::PoolTemplateDataMap& m_poolTemplates;

    explicit PoolMapChecker(PoolManager::PoolTemplateDataMap& poolTemplates) : m_poolTemplates(poolTemplates) {}

    bool CheckAndRemember(uint32 mapid, uint32 pool_id, char const* tableName, char const* elementName)
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
        if (!mapEntry)
        {
            return false;
        }

        MapEntry const* poolMapEntry = m_poolTemplates[pool_id].mapEntry;

        // if not listed then just remember
        if (!poolMapEntry)
        {
            m_poolTemplates[pool_id].mapEntry = mapEntry;
            return true;
        }

        // if at same map, then all ok
        if (poolMapEntry == mapEntry)
        {
            return true;
        }

        // pool spawns must be at single instanceable map
        if (mapEntry->Instanceable())
        {
            sLog.outErrorDb("`%s` has %s spawned at instanceable map %u when one or several other spawned at different map %u in pool id %i, skipped.",
                            tableName, elementName, mapid, poolMapEntry->ID, pool_id);
            return false;
        }

        // pool spawns must be at single instanceable map
        if (poolMapEntry->Instanceable())
        {
            sLog.outErrorDb("`%s` has %s spawned at map %u when one or several other spawned at different instanceable map %u in pool id %i, skipped.",
                            tableName, elementName, mapid, poolMapEntry->ID, pool_id);
            return false;
        }

        // pool spawns can be at different non-instanceable maps
        return true;
    }
};

/**
 * @brief Loads pool templates and members from the database.
 */
void PoolManager::LoadFromDB()
{
    std::optional<uint16> highestEntry = PoolDataRepository::QueryHighestPoolEntry();
    if (!highestEntry)
    {
        sLog.outString(">> Table pool_template is empty.");
        sLog.outString();
        return;
    }
    else
    {
        max_pool_id = *highestEntry;
    }

    mPoolTemplate.resize(max_pool_id + 1);

    std::vector<PoolTemplateRow> pools = PoolDataRepository::QueryPools();
    if (pools.empty())
    {
        mPoolTemplate.clear();
        sLog.outString(">> Table pool_template is empty:");
        sLog.outString();
        return;
    }

    uint32 count = 0;

    BarGoLink bar(pools.size());
    for (PoolTemplateRow const& row : pools)
    {
        ++count;

        bar.step();

        uint16 pool_id = row.entry;

        PoolTemplateData& pPoolTemplate = mPoolTemplate[pool_id];
        pPoolTemplate.MaxLimit    = row.maxLimit;
        pPoolTemplate.description = row.description;
        pPoolTemplate.AutoSpawn = true;          // will update and later data loading
    }

    sLog.outString();
    sLog.outString(">> Loaded %u objects pools", count);

    PoolMapChecker mapChecker(mPoolTemplate);

    // Creatures (guids and entries)

    mPoolCreatureGroups.resize(max_pool_id + 1);
    mCreatureSearchMap.clear();
    std::vector<PoolCreatureRow> poolCreatures = PoolDataRepository::QueryPoolCreatures();

    count = 0;
    if (poolCreatures.empty())
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature`", count);
    }
    else
    {
        BarGoLink bar2(poolCreatures.size());
        for (PoolCreatureRow const& row : poolCreatures)
        {
            bar2.step();

            uint32 guid    = row.guid;
            uint16 pool_id = row.poolEntry;
            float chance   = row.chance;

            CreatureData const* data = sObjectMgr.GetCreatureData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_creature` has a non existing creature spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_creature` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_creature` has an invalid chance (%f) for creature guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_creature", "creature guid"))
            {
                continue;
            }

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<Creature>& cregroup = mPoolCreatureGroups[pool_id];
            cregroup.SetPoolId(pool_id);
            cregroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mCreatureSearchMap.insert(p);
        }
        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature`", count);
    }

    std::vector<PoolCreatureTemplateRow> poolCreatureTemplates = PoolDataRepository::QueryPoolCreatureTemplates();

    count = 0;
    if (poolCreatureTemplates.empty())
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature_template`", count);
    }
    else
    {
        BarGoLink bar2(poolCreatureTemplates.size());
        for (PoolCreatureTemplateRow const& row : poolCreatureTemplates)
        {
            bar2.step();

            uint32 guid    = row.guid;
            uint16 pool_id = row.poolEntry;
            float chance   = row.chance;
            uint16 entry_id = row.id;        // for errors output only

            CreatureData const* data = sObjectMgr.GetCreatureData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_creature_template` has a non existing creature spawn (GUID: %u Entry: %u) defined for pool id (%u), skipped.", guid, entry_id, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_creature_template` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_creature_template` has an invalid chance (%f) for creature (Guid %u Entry %u) in pool id (%i), skipped.", chance, guid, entry_id, pool_id);
                continue;
            }

            // `pool_creature` and `pool_creature_template` can't have guids duplicates (in second case because entries also unique)
            // So if guid already listed in pools then this duplicate from alt.table
            // Also note: for added guid not important what case we skip from 2 tables
            if (uint16 alt_pool_id = IsPartOfAPool<Creature>(guid))
            {
                sLog.outErrorDb("`pool_creature` has guid %u for pool %u that already added to pool %u from `pool_creature_template` for creature entry %u, skipped.",
                                guid, pool_id, alt_pool_id, entry_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_creature_template", "creature guid"))
            {
                continue;
            }

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<Creature>& cregroup = mPoolCreatureGroups[pool_id];
            cregroup.SetPoolId(pool_id);
            cregroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mCreatureSearchMap.insert(p);
        }
        sLog.outString();
        sLog.outString(">> Loaded %u creatures in pools from `pool_creature_template`", count);
    }

    // Gameobjects (guids and entries)

    mPoolGameobjectGroups.resize(max_pool_id + 1);
    mGameobjectSearchMap.clear();
    std::vector<PoolGameObjectRow> poolGameObjects = PoolDataRepository::QueryPoolGameObjects();

    count = 0;
    if (poolGameObjects.empty())
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject`", count);
    }
    else
    {
        BarGoLink bar2(poolGameObjects.size());
        for (PoolGameObjectRow const& row : poolGameObjects)
        {
            bar2.step();

            uint32 guid    = row.guid;
            uint16 pool_id = row.poolEntry;
            float chance   = row.chance;

            GameObjectData const* data = sObjectMgr.GetGOData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_gameobject` has a non existing gameobject spawn (GUID: %u) defined for pool id (%u), skipped.", guid, pool_id);
                continue;
            }
            GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(data->id);
            if (goinfo->type != GAMEOBJECT_TYPE_CHEST &&
                    goinfo->type != GAMEOBJECT_TYPE_GOOBER &&
                    goinfo->type != GAMEOBJECT_TYPE_FISHINGHOLE &&
                    goinfo->type != GAMEOBJECT_TYPE_TRAP)
            {
                sLog.outErrorDb("`pool_gameobject` has a not lootable gameobject spawn (GUID: %u, type: %u) defined for pool id (%u), skipped.", guid, goinfo->type, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_gameobject` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_gameobject` has an invalid chance (%f) for gameobject guid (%u) in pool id (%i), skipped.", chance, guid, pool_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_gameobject", "gameobject guid"))
            {
                continue;
            }

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<GameObject>& gogroup = mPoolGameobjectGroups[pool_id];
            gogroup.SetPoolId(pool_id);
            gogroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mGameobjectSearchMap.insert(p);
        }
        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject`", count);
    }

    std::vector<PoolGameObjectTemplateRow> poolGameObjectTemplates = PoolDataRepository::QueryPoolGameObjectTemplates();

    count = 0;
    if (poolGameObjectTemplates.empty())
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject_template`", count);
    }
    else
    {
        BarGoLink bar2(poolGameObjectTemplates.size());
        for (PoolGameObjectTemplateRow const& row : poolGameObjectTemplates)
        {
            bar2.step();

            uint32 guid    = row.guid;
            uint16 pool_id = row.poolEntry;
            float chance   = row.chance;
            uint16 entry_id = row.id;        // for errors output only

            GameObjectData const* data = sObjectMgr.GetGOData(guid);
            if (!data)
            {
                sLog.outErrorDb("`pool_gameobject_template` has a non existing gameobject spawn (GUID: %u Entry %u) defined for pool id (%u), skipped.", guid, entry_id, pool_id);
                continue;
            }
            GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(data->id);
            if (goinfo->type != GAMEOBJECT_TYPE_CHEST &&
                    goinfo->type != GAMEOBJECT_TYPE_GOOBER &&
                    goinfo->type != GAMEOBJECT_TYPE_FISHINGHOLE &&
                    goinfo->type != GAMEOBJECT_TYPE_TRAP)
            {
                sLog.outErrorDb("`pool_gameobject_template` has a not lootable gameobject spawn (GUID: %u Entry %u Type: %u) defined for pool id (%u), skipped.", guid, entry_id, goinfo->type, pool_id);
                continue;
            }
            if (pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_gameobject_template` pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_gameobject_template` has an invalid chance (%f) for gameobject (Guid %u Entry %u) in pool id (%i), skipped.", chance, guid, entry_id, pool_id);
                continue;
            }

            // `pool_gameobject` and `pool_gameobject_template` can't have guids duplicates (in second case because entries also unique)
            // So if guid already listed in pools then this duplicate from alt.table
            // Also note: for added guid not important what case we skip from 2 tables
            if (uint16 alt_pool_id = IsPartOfAPool<GameObject>(guid))
            {
                sLog.outErrorDb("`pool_gameobject` has guid %u for pool %u that already added to pool %u from `pool_gameobject_template` for gameobject entry %u, skipped.",
                                guid, pool_id, alt_pool_id, entry_id);
                continue;
            }

            if (!mapChecker.CheckAndRemember(data->mapid, pool_id, "pool_gameobject_template", "gameobject guid"))
            {
                continue;
            }

            PoolTemplateData* pPoolTemplate = &mPoolTemplate[pool_id];

            ++count;

            PoolObject plObject = PoolObject(guid, chance);
            PoolGroup<GameObject>& gogroup = mPoolGameobjectGroups[pool_id];
            gogroup.SetPoolId(pool_id);
            gogroup.AddEntry(plObject, pPoolTemplate->MaxLimit);
            SearchPair p(guid, pool_id);
            mGameobjectSearchMap.insert(p);
        }
        sLog.outString();
        sLog.outString(">> Loaded %u gameobject in pools from `pool_gameobject_template`", count);
    }

    // Pool of pools
    mPoolPoolGroups.resize(max_pool_id + 1);
    std::vector<PoolPoolRow> poolPools = PoolDataRepository::QueryPoolPools();

    count = 0;
    if (poolPools.empty())
    {
        BarGoLink bar2(1);
        bar2.step();

        sLog.outString();
        sLog.outString(">> Loaded %u pools in pools", count);
    }
    else
    {
        BarGoLink bar2(poolPools.size());
        for (PoolPoolRow const& row : poolPools)
        {
            bar2.step();

            uint16 child_pool_id  = row.poolId;
            uint16 mother_pool_id = row.motherPool;
            float chance          = row.chance;

            if (mother_pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_pool` mother_pool id (%i) is out of range compared to max pool id in `pool_template`, skipped.", mother_pool_id);
                continue;
            }
            if (child_pool_id > max_pool_id)
            {
                sLog.outErrorDb("`pool_pool` included pool_id (%i) is out of range compared to max pool id in `pool_template`, skipped.", child_pool_id);
                continue;
            }
            if (mother_pool_id == child_pool_id)
            {
                sLog.outErrorDb("`pool_pool` pool_id (%i) includes itself, dead-lock detected, skipped.", child_pool_id);
                continue;
            }
            if (chance < 0 || chance > 100)
            {
                sLog.outErrorDb("`pool_pool` has an invalid chance (%f) for pool id (%u) in mother pool id (%i), skipped.", chance, child_pool_id, mother_pool_id);
                continue;
            }

            PoolTemplateData* pPoolTemplateMother = &mPoolTemplate[mother_pool_id];

            ++count;

            PoolObject plObject = PoolObject(child_pool_id, chance);
            PoolGroup<Pool>& plgroup = mPoolPoolGroups[mother_pool_id];
            plgroup.SetPoolId(mother_pool_id);
            plgroup.AddEntry(plObject, pPoolTemplateMother->MaxLimit);
            SearchPair p(child_pool_id, mother_pool_id);
            mPoolSearchMap.insert(p);

            // update top independent pool flag
            mPoolTemplate[child_pool_id].AutoSpawn = false;
        }

        // Now check for circular reference
        for (uint16 i = 0; i < max_pool_id; ++i)
        {
            std::set<uint16> checkedPools;
            for (SearchMap::iterator poolItr = mPoolSearchMap.find(i); poolItr != mPoolSearchMap.end(); poolItr = mPoolSearchMap.find(poolItr->second))
            {
                // if child pool not have map data then it empty or have not checked child then will checked and all line later
                if (MapEntry const* childMapEntry = mPoolTemplate[poolItr->first].mapEntry)
                {
                    if (!mapChecker.CheckAndRemember(childMapEntry->ID, poolItr->second, "pool_pool", "pool with creature/gameobject"))
                    {
                        mPoolPoolGroups[poolItr->second].RemoveOneRelation(poolItr->first);
                        mPoolSearchMap.erase(poolItr);
                        --count;
                        break;
                    }
                }

                checkedPools.insert(poolItr->first);
                if (checkedPools.find(poolItr->second) != checkedPools.end())
                {
                    std::ostringstream ss;
                    ss << "The pool(s) ";
                    for (std::set<uint16>::const_iterator itr = checkedPools.begin(); itr != checkedPools.end(); ++itr)
                    {
                        ss << *itr << " ";
                    }
                    ss << "create(s) a circular reference, which can cause the server to freeze.\nRemoving the last link between mother pool "
                       << poolItr->first << " and child pool " << poolItr->second;
                    sLog.outErrorDb("%s", ss.str().c_str());
                    mPoolPoolGroups[poolItr->second].RemoveOneRelation(poolItr->first);
                    mPoolSearchMap.erase(poolItr);
                    --count;
                    break;
                }
            }
        }

        sLog.outString();
        sLog.outString(">> Loaded %u pools in mother pools", count);
    }

    // check chances integrity
    for (uint16 pool_entry = 0; pool_entry < mPoolTemplate.size(); ++pool_entry)
    {
        if (mPoolTemplate[pool_entry].AutoSpawn)
        {
            if (!CheckPool(pool_entry))
            {
                sLog.outErrorDb("Pool Id (%u) has all creatures or gameobjects with explicit chance sum <>100 and no equal chance defined. The pool system cannot pick one to spawn.", pool_entry);
                mPoolTemplate[pool_entry].AutoSpawn = false;
            }
        }
    }
}

// Method that check chance integrity of the creatures and gameobjects in this pool
bool PoolManager::CheckPool(uint16 pool_id) const
{
    return pool_id <= max_pool_id &&
           mPoolGameobjectGroups[pool_id].CheckPool() &&
           mPoolCreatureGroups[pool_id].CheckPool() &&
           mPoolPoolGroups[pool_id].CheckPool();
}

// Method that check linking all elements to event
void PoolManager::CheckEventLinkAndReport(uint16 pool_id, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const
{
    mPoolGameobjectGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
    mPoolCreatureGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
    mPoolPoolGroups[pool_id].CheckEventLinkAndReport(event_id, creature2event, go2event);
}

// Method that exclude some elements from next spawn
template<>

/**
 * @brief Sets the exclusion state for a creature within a pool.
 *
 * @param pool_id The pool id.
 * @param db_guid_or_pool_id The creature guid.
 * @param state The exclusion state.
 */
void PoolManager::SetExcludeObject<Creature>(uint16 pool_id, uint32 db_guid_or_pool_id, bool state)
{
    mPoolCreatureGroups[pool_id].SetExcludeObject(db_guid_or_pool_id, state);
}

template<>

/**
 * @brief Sets the exclusion state for a gameobject within a pool.
 *
 * @param pool_id The pool id.
 * @param db_guid_or_pool_id The gameobject guid.
 * @param state The exclusion state.
 */
void PoolManager::SetExcludeObject<GameObject>(uint16 pool_id, uint32 db_guid_or_pool_id, bool state)
{
    mPoolGameobjectGroups[pool_id].SetExcludeObject(db_guid_or_pool_id, state);
}
