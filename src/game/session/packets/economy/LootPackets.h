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

#ifndef MANGOS_H_LOOTPACKETS
#define MANGOS_H_LOOTPACKETS

class ByteBuffer;
struct LootItem;
struct LootView;

/// Writes a loot item: an item's id, count, display, random suffix and random property, or a
/// currency's id and count.
ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li);

/// Writes what a loot window shows its viewer: the gold, then each item the viewer may see with
/// its slot and slot type, then the currencies; nothing but empty counts without permission.
ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv);

#endif
