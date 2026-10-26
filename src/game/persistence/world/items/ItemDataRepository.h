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

#ifndef MANGOS_H_ITEMDATAREPOSITORY
#define MANGOS_H_ITEMDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

class QueryResult;

/// One `item_convert` row.
struct ItemConvertRow
{
    uint32 entry = 0;
    uint32 item = 0;
};

/// One `item_expire_convert` row.
struct ItemExpireConvertRow
{
    uint32 entry = 0;
    uint32 item = 0;
};

/// One `item_required_target` row.
struct ItemRequiredTargetRow
{
    uint32 entry = 0;
    uint32 type = 0;
    uint32 targetEntry = 0;
};

/// One `hotfix_data` row.
struct HotfixRow
{
    uint32 entry = 0;
    uint32 type = 0;
    uint32 timestamp = 0;
};

/// The item data on the world database: `ObjectMgr`'s item loads and the hotfix list at startup and
/// on a reload, each run synchronously, and a GM's item look-up by name, the name escaped first.
namespace ItemDataRepository
{
    /// Selects every item's localised name and description (`locales_item`).
    QueryResult* QueryItemLocales();

    /// Selects every item conversion (`item_convert`).
    std::vector<ItemConvertRow> QueryItemConverts();

    /// Selects every expired item's conversion (`item_expire_convert`).
    std::vector<ItemExpireConvertRow> QueryItemExpireConverts();

    /// Selects every item's required spell target (`item_required_target`).
    std::vector<ItemRequiredTargetRow> QueryItemRequiredTargets();

    /// Selects every hotfix entry with its date (`hotfix_data`).
    std::vector<HotfixRow> QueryHotfixes();

    /// Selects an item template's entry by its name, the name escaped first.
    QueryResult* QueryItemEntryByName(std::string itemName);
}

#endif
