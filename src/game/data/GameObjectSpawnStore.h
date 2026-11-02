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

#ifndef MANGOS_H_GAMEOBJECTSPAWNSTORE
#define MANGOS_H_GAMEOBJECTSPAWNSTORE

#include "Platform/Define.h"
#include "data/GameObjectRecords.h"

#include <unordered_map>

typedef std::unordered_map<uint32, GameObjectData> GameObjectDataMap;
typedef GameObjectDataMap::value_type GameObjectDataPair;

/**
 * The game object spawns (`gameobject`), one per spawn guid. The loader, and a game object saving its
 * own spawn, fill a spawn through `FindOrAdd`, which answers the guid's spawn, an empty one when the
 * guid has none; a deleted spawn is erased.
 */
class GameObjectSpawnStore
{
    public:
        GameObjectDataPair const* Find(uint32 guid) const;
        GameObjectData& FindOrAdd(uint32 guid) { return m_spawns[guid]; }
        void Erase(uint32 guid) { m_spawns.erase(guid); }
        GameObjectDataMap const& All() const { return m_spawns; }

    private:
        GameObjectDataMap m_spawns;
};

#endif
