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

#ifndef MANGOS_H_LOOTDATAREPOSITORY
#define MANGOS_H_LOOTDATAREPOSITORY

#include "Platform/Define.h"

#include <vector>

class QueryResult;

/// One row of a loot template table (`creature_loot_template` and the tables like it).
struct LootTemplateRow
{
    uint32 entry = 0;
    int32 item = 0;
    float chanceOrQuestChance = 0.0f;
    uint8 groupid = 0;
    int32 mincountOrRef = 0;
    uint32 maxcount = 0;
    uint16 conditionId = 0;
};

/// One `item_enchantment_template` row.
struct ItemEnchantmentRow
{
    int32 entry = 0;
    uint32 ench = 0;
    float chance = 0.0f;
};

/// The loot and random enchantment data on the world database: the loot stores' and
/// `ItemEnchantmentMgr`'s loads at startup and on a reload, each run synchronously.
namespace LootDataRepository
{
    /// Selects every row of a loot template table.
    std::vector<LootTemplateRow> QueryLootTemplate(char const* table);

    /// Selects every random item enchantment (`item_enchantment_template`).
    std::vector<ItemEnchantmentRow> QueryItemEnchantments();

    /// Selects every item any loot table drops, for the auction house bot's filter.
    QueryResult* QueryLootItemEntries();
}

#endif
