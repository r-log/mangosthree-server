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

#include "data/CellObjectGuidStore.h"
#include "Utilities/PackedValues.h"

CellObjectGuids& CellObjectGuidStore::FindOrAddCell(uint32 mapId, uint8 spawnMode, uint32 cellId)
{
    return m_cells[MAKE_PAIR64(mapId, spawnMode)][cellId];
}

CellObjectGuids const* CellObjectGuidStore::FindCell(uint32 mapId, uint8 spawnMode, uint32 cellId) const
{
    MapObjectGuids::const_iterator mapItr = m_cells.find(MAKE_PAIR64(mapId, spawnMode));
    if (mapItr == m_cells.end())
    {
        return NULL;
    }
    CellObjectGuidsMap::const_iterator cellItr = mapItr->second.find(cellId);
    if (cellItr == mapItr->second.end())
    {
        return NULL;
    }
    return &cellItr->second;
}

void CellObjectGuidStore::RemoveActiveCreature(uint32 mapId, uint32 guid)
{
    std::pair<ActiveCreatureGuidsOnMap::iterator, ActiveCreatureGuidsOnMap::iterator> bounds = m_activeCreatures.equal_range(mapId);
    for (ActiveCreatureGuidsOnMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == guid)
        {
            m_activeCreatures.erase(itr);
            return;
        }
    }
}
