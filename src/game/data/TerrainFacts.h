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

#ifndef MANGOS_H_TERRAINFACTS
#define MANGOS_H_TERRAINFACTS

#include <optional>

#include "Platform/Define.h"

/**
 * What the data layer asks of the terrain the maps load: the zone a position is in and the static
 * floor under it. mangosd installs the terrain manager's facts (`TerrainManager::GetTerrainFacts`)
 * at start-up; the data layer asks the installed ones through sTerrainFacts. This base answers for
 * a core with no terrain: zone 0 and no floor.
 */
class TerrainFacts
{
    public:
        virtual ~TerrainFacts();

        /// The zone of the position on map `mapId`.
        virtual uint32 ZoneId(uint32 mapId, float x, float y, float z) const;
        /// The floor under the position on map `mapId`, static geometry only; none when the column is bare.
        virtual std::optional<float> StaticFloor(uint32 mapId, float x, float y, float z) const;

        /// The installed facts, or the base when none are installed.
        static TerrainFacts& Installed();
        /// Installs `facts` and answers the facts it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static TerrainFacts* Install(TerrainFacts* facts);
        /// Whether facts are installed.
        static bool IsInstalled();
};

#define sTerrainFacts TerrainFacts::Installed()

#endif
