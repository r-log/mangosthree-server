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

#include "SpawnFinders.h"

#include "Map.h"
#include "MapPersistentStateMgr.h"
#include "Player.h"
#include "PoolManager.h"

/**
 * @brief Evaluates whether a creature spawn matches the current search criteria.
 *
 * @param dataPair The creature data pair being tested.
 * @return true if the search can stop early; otherwise, false.
 */
bool FindCreatureData::operator()(CreatureDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
    {
        return true;
    }

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->Where().DistanceTo(Geometry::Vector2(dataPair.second.posX, dataPair.second.posY));

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state),
    uint16 pool_id = sPoolMgr.IsPartOfAPool<Creature>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<Creature>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

/**
 * @brief Gets the best matching creature spawn data found by the search.
 *
 * @return The selected creature data pair, or null if none matched.
 */
CreatureDataPair const* FindCreatureData::GetResult() const
{
    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    if (i_mapData)
    {
        return i_mapData;
    }

    return i_anyData;
}

/**
 * @brief Evaluates whether a gameobject spawn matches the current search criteria.
 *
 * @param dataPair The gameobject data pair being tested.
 * @return true if the search can stop early; otherwise, false.
 */
bool FindGOData::operator()(GameObjectDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
    {
        return true;
    }

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->Where().DistanceTo(Geometry::Vector2(dataPair.second.posX, dataPair.second.posY));

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state)
    uint16 pool_id = sPoolMgr.IsPartOfAPool<GameObject>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<GameObject>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

/**
 * @brief Gets the best matching gameobject spawn data found by the search.
 *
 * @return The selected gameobject data pair, or null if none matched.
 */
GameObjectDataPair const* FindGOData::GetResult() const
{
    if (i_mapData)
    {
        return i_mapData;
    }

    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    return i_anyData;
}
