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

#include "data/DungeonFinderStore.h"

void DungeonFinderStore::ClearRequirements()
{
    m_requirements.clear();
}

void DungeonFinderStore::ClearRewards()
{
    m_rewards.clear();
}

void DungeonFinderStore::ClearItems()
{
    m_items.clear();
}

void DungeonFinderStore::SetRequirements(uint32 key, DungeonFinderRequirements const& requirements)
{
    m_requirements[key] = requirements;
}

void DungeonFinderStore::SetRewards(uint32 level, DungeonFinderRewards const& rewards)
{
    m_rewards[level] = rewards;
}

void DungeonFinderStore::SetItems(uint32 id, DungeonFinderItems const& items)
{
    m_items[id] = items;
}

DungeonFinderRequirements const* DungeonFinderStore::FindRequirements(uint32 mapId, uint32 difficulty) const
{
    DungeonFinderRequirementsMap::const_iterator itr = m_requirements.find(MAKE_PAIR32(mapId, difficulty));
    if (itr != m_requirements.end())
    {
        return &itr->second;
    }
    return NULL;
}

DungeonFinderRewards const* DungeonFinderStore::FindRewards(uint32 level) const
{
    DungeonFinderRewardsMap::const_iterator itr = m_rewards.find(level);
    if (itr != m_rewards.end())
    {
        return &itr->second;
    }
    return NULL;
}
