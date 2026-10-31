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

#ifndef MANGOS_H_SPAWNFINDERS
#define MANGOS_H_SPAWNFINDERS

#include "ObjectMgr.h"

class Player;

/// The spawn of an entry nearest the player, for the commands that go to one: a spawned one on the
/// player's map first, then any on that map, then any at all.
class FindCreatureData
{
    public:
        FindCreatureData(uint32 id, Player* player) : i_id(id), i_player(player),
            i_anyData(NULL), i_mapData(NULL), i_mapDist(0.0f), i_spawnedData(NULL), i_spawnedDist(0.0f) {}

        bool operator()(CreatureDataPair const& dataPair);
        CreatureDataPair const* GetResult() const;

    private:
        uint32 i_id;
        Player* i_player;

        CreatureDataPair const* i_anyData;
        CreatureDataPair const* i_mapData;
        float i_mapDist;
        CreatureDataPair const* i_spawnedData;
        float i_spawnedDist;
};

/// The game object spawn of an entry nearest the player, for the commands that go to one: any on
/// the player's map first, then a spawned one there, then any at all.
class FindGOData
{
    public:
        FindGOData(uint32 id, Player* player) : i_id(id), i_player(player),
            i_anyData(NULL), i_mapData(NULL), i_mapDist(0.0f), i_spawnedData(NULL), i_spawnedDist(0.0f) {}

        bool operator()(GameObjectDataPair const& dataPair);
        GameObjectDataPair const* GetResult() const;

    private:
        uint32 i_id;
        Player* i_player;

        GameObjectDataPair const* i_anyData;
        GameObjectDataPair const* i_mapData;
        float i_mapDist;
        GameObjectDataPair const* i_spawnedData;
        float i_spawnedDist;
};

#endif
