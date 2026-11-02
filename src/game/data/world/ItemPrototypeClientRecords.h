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

#ifndef MANGOS_H_ITEMPROTOTYPECLIENTRECORDS
#define MANGOS_H_ITEMPROTOTYPECLIENTRECORDS

#include "Platform/Define.h"
#include "data/HotfixStore.h"

#include <set>

struct ItemPrototype;
struct ItemEntry;
struct ItemSparseEntry;

/// Takes the seven fields of the item's record in the client's Item.db2.
void TakeItemRecord(ItemPrototype& proto, ItemEntry const& record);

/**
 * Takes the fields of the item's record in the client's Item-sparse.db2: every one but the name and the
 * description, which the caller keeps in its text pool, and the inventory type, material and sheath, which
 * Item.db2 also carries. An empty stat type or spell id (-1 in the file) becomes 0, and an unlimited count or
 * stack (0x7FFFFFFF in the file) becomes -1, as the server reads them. The server-only fields stay as they are.
 */
void TakeItemSparseRecord(ItemPrototype& proto, ItemSparseEntry const& record);

/// The items whose Item-sparse.db2 record `hotfix_data` overrides: the server keeps their `item_sparse` rows.
std::set<uint32> ItemSparseOverrides(HotfixData const& hotfixes);

#endif