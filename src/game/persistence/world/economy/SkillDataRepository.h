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

#ifndef MANGOS_H_SKILLDATAREPOSITORY
#define MANGOS_H_SKILLDATAREPOSITORY

#include "Platform/Define.h"

#include <vector>

class QueryResult;

/// One `skill_fishing_base_level` row.
struct FishingBaseLevelRow
{
    uint32 entry = 0;
    int32 skill = 0;
};

/// The tradeskill data on the world database: the discovery and extra item loads at startup and on
/// a reload, each run synchronously.
namespace SkillDataRepository
{
    /// Selects every recipe discovery (`skill_discovery_template`).
    QueryResult* QueryDiscoveries();

    /// Selects every extra item chance (`skill_extra_item_template`).
    QueryResult* QueryExtraItems();

    /// Selects every fishing zone's base skill level (`skill_fishing_base_level`).
    std::vector<FishingBaseLevelRow> QueryFishingBaseLevels();
}

#endif
