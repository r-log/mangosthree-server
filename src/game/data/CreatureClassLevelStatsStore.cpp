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

#include "data/CreatureClassLevelStatsStore.h"

#include <cstring>

void CreatureClassLevelStatsStore::Clear()
{
    memset(&m_stats, 0, sizeof(m_stats));
}

bool CreatureClassLevelStatsStore::InRange(uint32 level, uint32 classIndex, uint32 expansion)
{
    return level <= DEFAULT_MAX_CREATURE_LEVEL && classIndex < MAX_CREATURE_CLASS && expansion <= MAX_EXPANSION;
}

void CreatureClassLevelStatsStore::Set(uint32 level, uint32 classIndex, uint32 expansion, CreatureClassLvlStats const& stats)
{
    if (InRange(level, classIndex, expansion))
    {
        m_stats[level][classIndex][expansion] = stats;
    }
}

CreatureClassLvlStats const* CreatureClassLevelStatsStore::Find(uint32 level, uint32 classIndex, uint32 expansion) const
{
    return InRange(level, classIndex, expansion) ? &m_stats[level][classIndex][expansion] : NULL;
}
