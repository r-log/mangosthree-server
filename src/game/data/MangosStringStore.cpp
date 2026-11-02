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

#include "data/MangosStringStore.h"

void MangosStringStore::EraseRange(int32 startEntry, int32 endEntry)
{
    for (std::unordered_map<int32, MangosStringLocale>::iterator itr = m_strings.begin(); itr != m_strings.end();)
    {
        if (itr->first >= startEntry && itr->first < endEntry)
        {
            m_strings.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }
}

MangosStringLocale const* MangosStringStore::Find(int32 entry) const
{
    std::unordered_map<int32, MangosStringLocale>::const_iterator itr = m_strings.find(entry);
    return itr != m_strings.end() ? &itr->second : NULL;
}

uint32 MangosStringStore::LoadedCount(int32 minEntry) const
{
    std::map<int32, uint32>::const_iterator itr = m_loadedCounts.find(minEntry);
    return itr != m_loadedCounts.end() ? itr->second : 0;
}
