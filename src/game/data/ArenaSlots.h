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

#ifndef MANGOS_H_ARENASLOTS
#define MANGOS_H_ARENASLOTS

#include "Platform/Define.h"
#include "SharedDefines.h"

/// The arena team slots of a character, one per arena type: 2v2, 3v3 and 5v5.

#define MAX_ARENA_SLOT 3                                    // 0..2 slots

/// The slot of an arena type; 0xFF, and an error logged, for a type no slot holds.
uint8 GetArenaSlotByType(ArenaType type);

/// The arena type of a slot; ArenaType(0xFF), and an error logged, for a slot past the last.
ArenaType GetArenaTypeBySlot(uint8 slot);

#endif
