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

#ifndef MANGOS_H_GAMETELESTORE
#define MANGOS_H_GAMETELESTORE

#include "Platform/Define.h"

#include <string>
#include <unordered_map>

struct GameTele
{
    float  position_x;
    float  position_y;
    float  position_z;
    float  orientation;
    uint32 mapId;
    std::string name;
    std::wstring wnameLow;
};

typedef std::unordered_map<uint32, GameTele > GameTeleMap;

/**
 * The named teleport locations (`game_tele`), one per id. Setting an id that already holds one
 * replaces it, an id with none answers NULL, erasing forgets one, clearing forgets every id.
 */
class GameTeleStore
{
    public:
        void Clear() { m_teles.clear(); }
        void Set(uint32 id, GameTele const& tele) { m_teles[id] = tele; }
        void Erase(uint32 id) { m_teles.erase(id); }
        GameTele const* Find(uint32 id) const;
        GameTeleMap const& All() const { return m_teles; }

    private:
        GameTeleMap m_teles;
};

#endif
