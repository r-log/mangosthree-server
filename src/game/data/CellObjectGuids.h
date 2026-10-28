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

#ifndef MANGOS_H_CELLOBJECTGUIDS
#define MANGOS_H_CELLOBJECTGUIDS

#include <map>
#include <set>
#include <unordered_map>

#include "Platform/Define.h"

/// The spawn guids of one kind filed under a grid cell.
typedef std::set<uint32> CellGuidSet;

typedef std::map < uint32/*player guid*/, uint32/*instance*/ > CellCorpseSet;
struct CellObjectGuids
{
    CellGuidSet creatures;
    CellGuidSet gameobjects;
    CellCorpseSet corpses;
};
typedef std::unordered_map < uint32/*cell_id*/, CellObjectGuids > CellObjectGuidsMap;
// Keyed on the 64-bit pair, not the 32-bit one: a map id is uint32 and the
// world database stores `creature`.`map` as int(10) unsigned, so packing it
// into sixteen bits wraps a large id onto another map's bucket instead of
// missing. See the warning above MAKE_PAIR32 in PackedValues.h.
typedef std::unordered_map < uint64/*(mapid,spawnMode) pair*/, CellObjectGuidsMap > MapObjectGuids;

#endif
