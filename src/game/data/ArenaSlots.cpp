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

#include "data/ArenaSlots.h"

#include "Log.h"

uint8 GetArenaSlotByType(ArenaType type)
{
    switch (type)
    {
        case ARENA_TYPE_2v2: return 0;
        case ARENA_TYPE_3v3: return 1;
        case ARENA_TYPE_5v5: return 2;
        default:
            break;
    }
    sLog.outError("FATAL: Unknown arena team type %u for some arena team", type);
    return 0xFF;
}

ArenaType GetArenaTypeBySlot(uint8 slot)
{
    switch (slot)
    {
        case 0: return ARENA_TYPE_2v2;
        case 1: return ARENA_TYPE_3v3;
        case 2: return ARENA_TYPE_5v5;
        default:
            break;
    }
    sLog.outError("FATAL: Unknown arena team slot %u for some arena team", slot);
    return ArenaType(0xFF);
}
