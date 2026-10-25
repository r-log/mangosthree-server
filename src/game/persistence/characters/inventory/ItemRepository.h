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

#ifndef MANGOS_H_ITEMREPOSITORY
#define MANGOS_H_ITEMREPOSITORY

#include <functional>
#include <string>
#include <vector>
#include "Platform/Define.h"

class QueryResult;

/// One saved loot row of an item: the item id (a currency's negated), its count, random suffix
/// and random property.
struct ItemLootRow
{
    int32 itemId = 0;
    uint8 count = 0;
    uint32 randomSuffix = 0;
    int32 randomPropertyId = 0;
};

/// The items' saved rows (`item_instance`, `character_gifts`, `item_loot`, and an item's
/// `character_inventory` row). The gift read is asynchronous, and the start-up's highest guid and
/// a GM's item list reads synchronous; every write is queued on the character database, and one called inside
/// CharacterTransaction joins it.
namespace ItemRepository
{
    /// Asks for a wrapped item's gift row: the item's entry and flags before it was wrapped. The
    /// callback runs on the world thread with the result, which it owns.
    void QueryGift(uint32 itemGuid, std::function<void(QueryResult*)> callback);

    /// Inserts a wrapped item's gift row: its owner, the item, and its entry and flags before it
    /// was wrapped.
    void InsertGift(uint32 ownerGuid, uint32 itemGuid, uint32 entry, uint32 flags);

    /// Deletes an item's instance row.
    void DeleteInstance(uint32 itemGuid);

    /// Deletes the instance row of an item whose template no longer exists, as a character's
    /// delete and load do.
    void DeleteUnknownItem(uint32 itemGuid);

    /// Each deletes a row the load drops: the inventory row of an item it cannot place, and the
    /// saved loot of an item the character no longer has.
    void DropInventoryRow(uint32 itemGuid);
    void DropLoot(uint32 itemGuid);

    /// Inserts an item's instance row: its owner, its values as text and its written text.
    void InsertInstance(uint32 itemGuid, uint32 ownerGuid, std::string const& data, std::string const& text);

    /// Updates an item's instance row: its values, owner and written text.
    void UpdateInstance(uint32 itemGuid, uint32 ownerGuid, std::string const& data, std::string const& text);

    /// Updates an item's values and owner, as the load does after fixing an item.
    void UpdateInstanceData(uint32 itemGuid, uint32 ownerGuid, std::string const& data);

    /// Gives a wrapped item's gift row to its owner.
    void SetGiftOwner(uint32 itemGuid, uint32 ownerGuid);

    /// Deletes a wrapped item's gift row.
    void DeleteGift(uint32 itemGuid);

    /// Deletes an item's saved loot.
    void DeleteLoot(uint32 itemGuid);

    /// Inserts the money row of an item's saved loot.
    void InsertLootMoney(uint32 itemGuid, uint32 ownerGuid, uint32 money);

    /// Inserts the item and currency rows of an item's saved loot, in the order handed, through one
    /// statement.
    void InsertLootItems(uint32 itemGuid, uint32 ownerGuid, std::vector<ItemLootRow> const& rows);

    /// Deletes one item's row of an item's saved loot.
    void DeleteLootItem(uint32 itemGuid, uint32 lootItemId);

    /// Deletes one currency's row of an item's saved loot (saved under the negated id).
    void DeleteLootCurrency(uint32 itemGuid, uint32 currencyId);

    /// Deletes the inventory row that holds an item.
    void DeleteInventoryRow(uint32 itemGuid);

    /// Each writes the inventory row that holds an item: its owner, bag, slot and template.
    void InsertInventoryRow(uint32 ownerGuid, uint32 bagGuid, uint8 slot, uint32 itemGuid, uint32 itemEntry);
    void UpdateInventoryRow(uint32 ownerGuid, uint32 bagGuid, uint8 slot, uint32 itemGuid, uint32 itemEntry);

    /// Selects the highest item guid, at start-up.
    QueryResult* QueryHighestGuid();

    /// Deletes the inventory, mail item, auction and guild bank rows of the item guids from the first
    /// unused one up, in one transaction, at start-up.
    void DeleteRowsOfMissingItems(uint32 firstUnusedGuid);

    /// Selects how many inventory rows hold an item entry, for a GM's item list.
    QueryResult* QueryInventoryCountOf(uint32 itemId);

    /// Selects up to count inventory rows holding an item entry, with their bag, owner and account.
    QueryResult* QueryInventoryHoldersOf(uint32 itemId, uint32 count);

    /// Selects how many mailed items are of an item entry.
    QueryResult* QueryMailCountOf(uint32 itemId);

    /// Selects up to count mailed items of an item entry, with their sender and receiver.
    QueryResult* QueryMailHoldersOf(uint32 itemId, uint32 count);

    /// Selects how many auctions are of an item entry.
    QueryResult* QueryAuctionCountOf(uint32 itemId);

    /// Selects up to count auctions of an item entry, with their owner.
    QueryResult* QueryAuctionHoldersOf(uint32 itemId, uint32 count);

    /// Selects how many guild bank items are of an item entry.
    QueryResult* QueryGuildBankCountOf(uint32 itemId);

    /// Selects up to count guild bank items of an item entry, with their guild.
    QueryResult* QueryGuildBankHoldersOf(uint32 itemId, uint32 count);
}

#endif
