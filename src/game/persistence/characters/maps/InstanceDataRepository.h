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

#ifndef MANGOS_H_INSTANCEDATAREPOSITORY
#define MANGOS_H_INSTANCEDATAREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// One `instance` row: an instance's script data, none when the column is NULL.
struct InstanceDataRow
{
    uint32 id = 0;
    uint32 map = 0;
    std::optional<std::string> data;
};

/// One `world` row: a world map's script data, none when the column is NULL.
struct WorldMapDataRow
{
    uint32 map = 0;
    std::optional<std::string> data;
};

/// The instance scripts' saved data: an instance's in `instance`.`data`, a world map's in
/// `world`.`data`. The startup reads run synchronously; every write is queued on the character
/// database.
namespace InstanceDataRepository
{
    /// Selects every instance's id, map and script data.
    std::vector<InstanceDataRow> QueryInstanceData();

    /// Selects every world map's script data.
    std::vector<WorldMapDataRow> QueryWorldData();

    /// Saves an instance's script data, the data a bound parameter.
    void SetInstanceData(std::string const& data, uint32 instanceId);

    /// Saves a world map's script data, the data a bound parameter.
    void SetWorldData(std::string const& data, uint32 mapId);

    /// Inserts a world map's row with empty script data.
    void InsertEmptyWorldData(uint32 mapId);
}

#endif
