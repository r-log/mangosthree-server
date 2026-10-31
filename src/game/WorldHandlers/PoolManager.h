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

#ifndef MANGOS_POOLHANDLER_H
#define MANGOS_POOLHANDLER_H

#include <cmath>
#include <utility>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include "DBCStructure.h"

class Creature;
class GameObject;

struct PoolTemplateData
{
    PoolTemplateData() : mapEntry(NULL), MaxLimit(0), AutoSpawn(false) {}

    MapEntry const* mapEntry;                               // Map id used for pool creature/gameobject spams. In case non-instanceable map
    // it can be not unique but base at sharing same pool system dynamic data in this case this is not important.
    // NULL is no spawns by some reason
    uint32  MaxLimit;
    bool AutoSpawn;                                         // spawn at pool system start (not part of another pool and not part of event spawn)
    std::string description;

    // helpers
    bool CanBeSpawnedAtMap(MapEntry const* entry) const
    {
        return mapEntry && (mapEntry == entry || (!entry->Instanceable() && !mapEntry->Instanceable()));
    }
};

struct PoolObject
{
    uint32  guid;
    float   chance;
    bool exclude;

    PoolObject(uint32 _guid, float _chance): guid(_guid), chance(fabs(_chance)), exclude(false) {}

    template<typename T>
    void CheckEventLinkAndReport(uint32 poolId, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;
};

class Pool                                                  // for Pool of Pool case
{
};

typedef std::vector<PoolObject> PoolObjectList;

template <class T>
class PoolGroup
{
    public:
        explicit PoolGroup() : poolId(0) { }
        void SetPoolId(uint32 pool_id) { poolId = pool_id; }
        uint32 GetPoolId() const { return poolId; }
        ~PoolGroup() {};
        bool isEmpty() const { return ExplicitlyChanced.empty() && EqualChanced.empty(); }
        void AddEntry(PoolObject& poolitem, uint32 maxentries);
        bool CheckPool() const;
        void CheckEventLinkAndReport(int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;
        void SetExcludeObject(uint32 guid, bool state);
        void RemoveOneRelation(uint16 child_pool_id);

        PoolObjectList const& GetExplicitlyChanced() const { return ExplicitlyChanced; }
        PoolObjectList const& GetEqualChanced() const { return EqualChanced; }

        size_t size() const { return ExplicitlyChanced.size() + EqualChanced.size(); }
    private:
        uint32 poolId;
        PoolObjectList ExplicitlyChanced;
        PoolObjectList EqualChanced;
};

class PoolManager
{
    public:
        PoolManager();
        ~PoolManager() {};

        void LoadFromDB();

        uint16 GetMaxPoolId() const { return max_pool_id; }

        template<typename T>
        uint16 IsPartOfAPool(uint32 db_guid_or_pool_id) const;

        // Method that tell if the creature/gameobject/pool is part of top level pool and return the pool id if yes
        template<typename T>
        uint16 IsPartOfTopPool(uint32 db_guid_or_pool_id) const
        {
            if (uint16 pool_id = IsPartOfAPool<T>(db_guid_or_pool_id))
            {
                if (uint16 top_pool_id = IsPartOfTopPool<Pool>(pool_id))
                {
                    return top_pool_id;
                }

                return pool_id;
            }

            return 0;
        }

        template<typename T>
        void SetExcludeObject(uint16 pool_id, uint32 db_guid_or_pool_id, bool state);

        bool CheckPool(uint16 pool_id) const;
        void CheckEventLinkAndReport(uint16 pool_id, int16 event_id, std::map<uint32, int16> const& creature2event, std::map<uint32, int16> const& go2event) const;

        void RemoveAutoSpawnForPool(uint16 pool_id) { mPoolTemplate[pool_id].AutoSpawn = false; }

        typedef std::vector<PoolTemplateData> PoolTemplateDataMap;
        PoolTemplateDataMap const& GetPoolTemplates() const { return mPoolTemplate; }
        PoolTemplateData const& GetPoolTemplate(uint16 pool_id) const { return mPoolTemplate[pool_id]; }
        PoolGroup<Creature> const& GetPoolCreatures(uint16 pool_id) const  { return mPoolCreatureGroups[pool_id]; }
        PoolGroup<GameObject> const& GetPoolGameObjects(uint16 pool_id) const  { return mPoolGameobjectGroups[pool_id]; }
        PoolGroup<Pool> const& GetPoolPools(uint16 pool_id) const  { return mPoolPoolGroups[pool_id]; }
    protected:
        uint16 max_pool_id;

        typedef std::vector<PoolGroup<Creature> >   PoolGroupCreatureMap;
        typedef std::vector<PoolGroup<GameObject> > PoolGroupGameObjectMap;
        typedef std::vector<PoolGroup<Pool> >       PoolGroupPoolMap;
        typedef std::pair<uint32, uint16> SearchPair;
        typedef std::map<uint32, uint16> SearchMap;

        PoolTemplateDataMap mPoolTemplate;
        PoolGroupCreatureMap mPoolCreatureGroups;
        PoolGroupGameObjectMap mPoolGameobjectGroups;
        PoolGroupPoolMap mPoolPoolGroups;

        // static maps DB low guid -> pool id
        SearchMap mCreatureSearchMap;
        SearchMap mGameobjectSearchMap;
        SearchMap mPoolSearchMap;
};

#define sPoolMgr MaNGOS::Singleton<PoolManager>::Instance()

// Method that tell if the creature is part of a pool and return the pool id if yes
template<>
inline uint16 PoolManager::IsPartOfAPool<Creature>(uint32 db_guid) const
{
    SearchMap::const_iterator itr = mCreatureSearchMap.find(db_guid);
    if (itr != mCreatureSearchMap.end())
    {
        return itr->second;
    }

    return 0;
}

// Method that tell if the gameobject is part of a pool and return the pool id if yes
template<>
inline uint16 PoolManager::IsPartOfAPool<GameObject>(uint32 db_guid) const
{
    SearchMap::const_iterator itr = mGameobjectSearchMap.find(db_guid);
    if (itr != mGameobjectSearchMap.end())
    {
        return itr->second;
    }

    return 0;
}

// Method that tell if the pool is part of another pool and return the pool id if yes
template<>
inline uint16 PoolManager::IsPartOfAPool<Pool>(uint32 pool_id) const
{
    SearchMap::const_iterator itr = mPoolSearchMap.find(pool_id);
    if (itr != mPoolSearchMap.end())
    {
        return itr->second;
    }

    return 0;
}

#endif
