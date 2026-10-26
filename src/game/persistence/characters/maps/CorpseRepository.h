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

#ifndef MANGOS_H_CORPSEREPOSITORY
#define MANGOS_H_CORPSEREPOSITORY

#include "Platform/Define.h"

#include <optional>
#include <string>
#include <vector>

/// A corpse's saved row, in the order of the INSERT's columns, each value of the type it is
/// written as.
struct CorpseRow
{
    uint32 guid = 0;
    uint32 player = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    uint32 map = 0;
    uint64 time = 0;
    uint32 type = 0;
    int instance = 0;
    uint32 phaseMask = 0;
};

/// One corpse as the startup load reads it: its `corpse` row, its owner's appearance and flags, and
/// the owner's guild when there is one.
struct CorpseDataRow
{
    uint32 guid = 0;
    uint32 player = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint32 map = 0;
    uint64 time = 0;
    uint32 corpseType = 0;
    uint32 instance = 0;
    uint32 phaseMask = 0;
    uint8 gender = 0;
    uint8 race = 0;
    uint8 classId = 0;
    uint32 playerBytes = 0;
    uint32 playerBytes2 = 0;
    std::string equipmentCache;
    uint32 guildId = 0;
    uint32 playerFlags = 0;
};

/// The players' corpses (`corpse`). The start-up reads run synchronously; every write is queued on
/// the character database, and one called inside CharacterTransaction joins it.
namespace CorpseRepository
{
    /// Inserts a corpse's row.
    void Insert(CorpseRow const& row);

    /// Deletes a player's corpses that are not bones.
    void DeleteOfPlayer(uint32 player);

    /// Selects the highest corpse guid, at start-up.
    std::optional<uint32> QueryHighestGuid();

    /// Selects every corpse that is not bones with its player's appearance and guild, at start-up.
    std::vector<CorpseDataRow> QueryCorpses();

    /// Deletes the bones and the corpses older than a number of seconds, at start-up.
    void DeleteBonesAndOldCorpses(uint32 maxAge);
}

#endif
