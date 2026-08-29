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

#ifndef MANGOS_PACKEDVALUES_H
#define MANGOS_PACKEDVALUES_H

#include "Platform/Define.h"

/**
 * @file
 * @brief Packing two 16-bit halves into one 32-bit value, and back.
 *
 * Used for composite keys -- map/zone, entry/index -- where the database and the
 * client both expect the halves fused into a single integer.
 *
 * MIND THE uint16 IN MAKE_PAIR32. Both halves are truncated to sixteen bits, so
 * a key built from a value that does not fit does not overflow into the high
 * half -- it wraps, and lands on a bucket that belongs to some other pair. The
 * lookup then succeeds and returns the wrong thing.
 *
 * A map id is uint32 everywhere in this tree, and `creature`.`map` and
 * `gameobject`.`map` are `int(10) unsigned` in the world database, so a map id
 * above 65535 is something the schema already permits and this macro already
 * silently mangles. Key on the 64-bit pair when either half can exceed 16 bits.
 */

#define MAKE_PAIR32(l, h)  uint32(uint16(l) | (uint32(h) << 16))
#define PAIR32_HIPART(x)   uint16((uint32(x) >> 16) & 0x0000FFFF)
#define PAIR32_LOPART(x)   uint16(uint32(x)         & 0x0000FFFF)

#define MAKE_PAIR64(l, h)  uint64(uint32(l) | (uint64(h) << 32))
#define PAIR64_HIPART(x)   uint32((uint64(x) >> 32) & UI64LIT(0x00000000FFFFFFFF))
#define PAIR64_LOPART(x)   uint32(uint64(x)         & UI64LIT(0x00000000FFFFFFFF))

#endif
