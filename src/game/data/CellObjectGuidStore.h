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

#ifndef MANGOS_H_CELLOBJECTGUIDSTORE
#define MANGOS_H_CELLOBJECTGUIDSTORE

#include "Platform/Define.h"
#include "data/CellObjectGuids.h"

#include <map>

/**
 * The spawn guids filed by where they load: per map, spawn mode and grid cell, the creature and game
 * object spawns and the player corpses in that cell (a corpse always under spawn mode 0), and per
 * map, the active creature spawns, loaded with the map, a deleted spawn removed. `FindOrAddCell` answers a cell for an edit,
 * adding an empty one when the cell has none; `FindCell` never adds one.
 */
class CellObjectGuidStore
{
    public:
        typedef std::multimap<uint32 /*mapId*/, uint32 /*guid*/> ActiveCreatureGuidsOnMap;

        CellObjectGuids& FindOrAddCell(uint32 mapId, uint8 spawnMode, uint32 cellId);
        CellObjectGuids const* FindCell(uint32 mapId, uint8 spawnMode, uint32 cellId) const;

        void AddActiveCreature(uint32 mapId, uint32 guid) { m_activeCreatures.insert(ActiveCreatureGuidsOnMap::value_type(mapId, guid)); }
        void RemoveActiveCreature(uint32 mapId, uint32 guid);
        ActiveCreatureGuidsOnMap const& ActiveCreatures() const { return m_activeCreatures; }

    private:
        MapObjectGuids m_cells;
        ActiveCreatureGuidsOnMap m_activeCreatures;
};

#endif
