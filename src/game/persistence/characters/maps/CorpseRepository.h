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

/// The players' corpses (`corpse`). Every write is queued on the character database, and one
/// called inside CharacterTransaction joins it.
namespace CorpseRepository
{
    /// Inserts a corpse's row.
    void Insert(CorpseRow const& row);

    /// Deletes a player's corpses that are not bones.
    void DeleteOfPlayer(uint32 player);
}

#endif
