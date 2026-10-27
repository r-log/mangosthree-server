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

#ifndef MANGOS_H_CHARACTERCLEANUPREPOSITORY
#define MANGOS_H_CHARACTERCLEANUPREPOSITORY

#include "Platform/Define.h"

#include <vector>

/// The character database cleanup at start-up: the distinct ids a table holds in a column, the
/// delete of the rows holding ids the stores do not know, and the delete of talent rows past the
/// spec and rank limits. The reads run synchronously; the deletes are queued, except the talent
/// delete, which runs at once.
namespace CharacterCleanupRepository
{
    /// Selects every distinct value of a column of a table.
    std::vector<uint32> QueryDistinct(char const* column, char const* table);

    /// Deletes, in one statement, every row of a table whose column holds one of the ids.
    void DeleteRowsWithIds(char const* table, char const* column, std::vector<uint32> const& ids);

    /// Deletes, at once, the talent rows past the spec count or the rank limit.
    void DeleteTalentsPastLimits(uint32 maxSpecCount, uint32 maxRank);
}

#endif
