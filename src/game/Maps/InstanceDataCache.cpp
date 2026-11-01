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

#include "InstanceDataCache.h"

#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "persistence/characters/maps/InstanceDataRepository.h"

void InstanceDataCache::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    m_instances.clear();
    m_worlds.clear();

    std::vector<InstanceDataRow> instances = InstanceDataRepository::QueryInstanceData();
    for (InstanceDataRow const& instance : instances)
    {
        CachedRow row;
        row.mapId = instance.map;
        row.isNull = !instance.data;
        row.value = instance.data.value_or(std::string());
        m_instances[instance.id] = row;
    }

    std::vector<WorldMapDataRow> worlds = InstanceDataRepository::QueryWorldData();
    for (WorldMapDataRow const& world : worlds)
    {
        CachedRow row;
        row.isNull = !world.data;
        row.value = world.data.value_or(std::string());
        m_worlds[world.map] = row;
    }

    sLog.outString(">> Loaded script data for %zu instance(s) and %zu world map(s)",
                   m_instances.size(), m_worlds.size());
}

InstanceScriptData InstanceDataCache::GetInstance(uint32 instanceId) const
{
    InstanceScriptData answer;

    std::shared_lock<std::shared_mutex> guard(m_lock);
    std::unordered_map<uint32, CachedRow>::const_iterator itr = m_instances.find(instanceId);
    if (itr != m_instances.end())
    {
        answer.present = true;
        answer.isNull = itr->second.isNull;
        answer.value = itr->second.value;
    }
    return answer;
}

InstanceScriptData InstanceDataCache::GetWorld(uint32 mapId) const
{
    InstanceScriptData answer;

    std::shared_lock<std::shared_mutex> guard(m_lock);
    std::unordered_map<uint32, CachedRow>::const_iterator itr = m_worlds.find(mapId);
    if (itr != m_worlds.end())
    {
        answer.present = true;
        answer.isNull = itr->second.isNull;
        answer.value = itr->second.value;
    }
    return answer;
}

size_t InstanceDataCache::InstanceCount() const
{
    std::shared_lock<std::shared_mutex> guard(m_lock);
    return m_instances.size();
}

size_t InstanceDataCache::WorldCount() const
{
    std::shared_lock<std::shared_mutex> guard(m_lock);
    return m_worlds.size();
}

void InstanceDataCache::SetInstance(uint32 instanceId, std::string const& data)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    CachedRow& row = m_instances[instanceId];               // creates the row on an INSERT
    row.isNull = false;
    row.value = data;
}

void InstanceDataCache::SetWorld(uint32 mapId, std::string const& data)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    CachedRow& row = m_worlds[mapId];
    row.isNull = false;
    row.value = data;
}

void InstanceDataCache::UpdateInstance(uint32 instanceId, std::string const& data)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    std::unordered_map<uint32, CachedRow>::iterator itr = m_instances.find(instanceId);
    if (itr != m_instances.end())                           // an UPDATE of no row changes nothing
    {
        itr->second.isNull = false;
        itr->second.value = data;
    }
}

void InstanceDataCache::UpdateWorld(uint32 mapId, std::string const& data)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    std::unordered_map<uint32, CachedRow>::iterator itr = m_worlds.find(mapId);
    if (itr != m_worlds.end())
    {
        itr->second.isNull = false;
        itr->second.value = data;
    }
}

void InstanceDataCache::SetInstanceMap(uint32 instanceId, uint32 mapId)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);
    m_instances[instanceId].mapId = mapId;
}

void InstanceDataCache::RemoveInstance(uint32 instanceId)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);
    m_instances.erase(instanceId);
}

void InstanceDataCache::RemoveInstancesOfMap(uint32 mapId)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    for (std::unordered_map<uint32, CachedRow>::iterator itr = m_instances.begin(); itr != m_instances.end();)
    {
        if (itr->second.mapId == mapId)
        {
            itr = m_instances.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void InstanceDataCache::Clear()
{
    std::unique_lock<std::shared_mutex> guard(m_lock);
    m_instances.clear();
    m_worlds.clear();
}
