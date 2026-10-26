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

#include "ItemRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

#include <utility>

namespace ItemRepository
{
    void QueryGift(uint32 itemGuid, std::function<void(QueryResult*)> callback)
    {
        CharacterDatabase.AsyncPQuery(std::move(callback),
                                      "SELECT `entry`, `flags` FROM `character_gifts` WHERE `item_guid` = '%u'",
                                      itemGuid);
    }

    void InsertGift(uint32 ownerGuid, uint32 itemGuid, uint32 entry, uint32 flags)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_gifts` VALUES ('%u', '%u', '%u', '%u')", ownerGuid, itemGuid, entry, flags);
    }

    void DeleteInstance(uint32 itemGuid)
    {
        static SqlStatementID delItem;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
        stmt.PExecute(itemGuid);
    }

    void DeleteUnknownItem(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", itemGuid);
    }

    void DropInventoryRow(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", itemGuid);
    }

    void DropLoot(uint32 itemGuid)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u'", itemGuid);
    }

    void InsertInstance(uint32 itemGuid, uint32 ownerGuid, std::string const& data, std::string const& text)
    {
        static SqlStatementID insItem;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insItem, "INSERT INTO `item_instance` (`guid`,`owner_guid`,`data`,`text`) VALUES (?, ?, ?, ?)");
        stmt.PExecute(itemGuid, ownerGuid, data.c_str(), text.c_str());
    }

    void UpdateInstance(uint32 itemGuid, uint32 ownerGuid, std::string const& data, std::string const& text)
    {
        static SqlStatementID updInstance;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updInstance, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ?, `text` = ? WHERE `guid` = ?");
        stmt.PExecute(data.c_str(), ownerGuid, text.c_str(), itemGuid);
    }

    void UpdateInstanceData(uint32 itemGuid, uint32 ownerGuid, std::string const& data)
    {
        static SqlStatementID updItem;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updItem, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ? WHERE `guid` = ?");
        stmt.addString(data);
        stmt.addUInt32(ownerGuid);
        stmt.addUInt32(itemGuid);
        stmt.Execute();
    }

    void SetGiftOwner(uint32 itemGuid, uint32 ownerGuid)
    {
        static SqlStatementID updGifts;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updGifts, "UPDATE `character_gifts` SET `guid` = ? WHERE `item_guid` = ?");
        stmt.PExecute(ownerGuid, itemGuid);
    }

    void DeleteGift(uint32 itemGuid)
    {
        static SqlStatementID delGifts;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
        stmt.PExecute(itemGuid);
    }

    void DeleteLoot(uint32 itemGuid)
    {
        static SqlStatementID delLoot;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM `item_loot` WHERE `guid` = ?");
        stmt.PExecute(itemGuid);
    }

    void InsertLootMoney(uint32 itemGuid, uint32 ownerGuid, uint32 money)
    {
        static SqlStatementID saveGold;
        SqlStatement stmt = CharacterDatabase.CreateStatement(saveGold, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`suffix`,`property`) VALUES (?, ?, 0, ?, 0, 0)");
        stmt.PExecute(itemGuid, ownerGuid, money);
    }

    void InsertLootItems(uint32 itemGuid, uint32 ownerGuid, std::vector<ItemLootRow> const& rows)
    {
        static SqlStatementID saveLoot;
        SqlStatement stmt = CharacterDatabase.CreateStatement(saveLoot, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`suffix`,`property`) VALUES (?, ?, ?, ?, ?, ?)");

        for (std::vector<ItemLootRow>::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
        {
            stmt.addUInt32(itemGuid);
            stmt.addUInt32(ownerGuid);
            stmt.addInt32(itr->itemId);
            stmt.addUInt8(itr->count);
            stmt.addUInt32(itr->randomSuffix);
            stmt.addInt32(itr->randomPropertyId);

            stmt.Execute();
        }
    }

    void DeleteLootItem(uint32 itemGuid, uint32 lootItemId)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u' AND `itemid` = '%u'", itemGuid, lootItemId);
    }

    void DeleteLootCurrency(uint32 itemGuid, uint32 currencyId)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u' AND `itemid` = '%i'", itemGuid, -int32(currencyId));
    }

    void DeleteInventoryRow(uint32 itemGuid)
    {
        static SqlStatementID delInv;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM `character_inventory` WHERE `item` = ?");
        stmt.PExecute(itemGuid);
    }

    void InsertInventoryRow(uint32 ownerGuid, uint32 bagGuid, uint8 slot, uint32 itemGuid, uint32 itemEntry)
    {
        static SqlStatementID insertInventory ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insertInventory, "INSERT INTO `character_inventory` (`guid`,`bag`,`slot`,`item`,`item_template`) VALUES (?, ?, ?, ?, ?)");
        stmt.addUInt32(ownerGuid);
        stmt.addUInt32(bagGuid);
        stmt.addUInt8(slot);
        stmt.addUInt32(itemGuid);
        stmt.addUInt32(itemEntry);
        stmt.Execute();
    }

    void UpdateInventoryRow(uint32 ownerGuid, uint32 bagGuid, uint8 slot, uint32 itemGuid, uint32 itemEntry)
    {
        static SqlStatementID updateInventory ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updateInventory, "UPDATE `character_inventory` SET `guid` = ?, `bag` = ?, `slot` = ?, `item_template` = ? WHERE `item` = ?");
        stmt.addUInt32(ownerGuid);
        stmt.addUInt32(bagGuid);
        stmt.addUInt8(slot);
        stmt.addUInt32(itemEntry);
        stmt.addUInt32(itemGuid);
        stmt.Execute();
    }

    std::optional<uint32> QueryHighestGuid()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `item_instance`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    void DeleteRowsOfMissingItems(uint32 firstUnusedGuid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` >= '%u'", firstUnusedGuid);
        CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` >= '%u'", firstUnusedGuid);
        CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `itemguid` >= '%u'", firstUnusedGuid);
        CharacterDatabase.PExecute("DELETE FROM `guild_bank_item` WHERE `item_guid` >= '%u'", firstUnusedGuid);
        CharacterDatabase.CommitTransaction();
    }

    QueryResult* QueryInventoryCountOf(uint32 itemId)
    {
        return CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `character_inventory` WHERE `item_template`='%u'", itemId);
    }

    QueryResult* QueryInventoryHoldersOf(uint32 itemId, uint32 count)
    {
        //                                          0        1             2             3        4                  5
        return CharacterDatabase.PQuery("SELECT `ci`.`item`, `cibag`.`slot` AS bag, `ci`.`slot`, `ci`.`guid`, `characters`.`account`,`characters`.`name` "
                                        "FROM `character_inventory` AS `ci` LEFT JOIN `character_inventory` AS cibag ON (`cibag`.`item`=`ci`.`bag`),`characters` "
                                        "WHERE `ci`.`item_template`='%u' AND `ci`.`guid` = `characters`.`guid` LIMIT %u ",
                                        itemId, count);
    }

    QueryResult* QueryMailCountOf(uint32 itemId)
    {
        return CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `mail_items` WHERE `item_template`='%u'", itemId);
    }

    QueryResult* QueryMailHoldersOf(uint32 itemId, uint32 count)
    {
        //                                          0                     1            2              3               4            5               6
        return CharacterDatabase.PQuery("SELECT `mail_items`.`item_guid`, `mail`.`sender`, `mail`.`receiver`, `char_s`.`account`, `char_s`.`name`, `char_r`.`account`, `char_r`.`name` "
                                        "FROM `mail`,`mail_items`,`characters` as char_s,`characters` as char_r "
                                        "WHERE `mail_items`.`item_template`='%u' AND `char_s`.`guid` = `mail`.`sender` AND `char_r`.`guid` = `mail`.`receiver` AND `mail`.`id`=`mail_items`.`mail_id` LIMIT %u",
                                        itemId, count);
    }

    QueryResult* QueryAuctionCountOf(uint32 itemId)
    {
        return CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `auction` WHERE `item_template`='%u'", itemId);
    }

    QueryResult* QueryAuctionHoldersOf(uint32 itemId, uint32 count)
    {
        //                                           0                      1                       2                   3
        return CharacterDatabase.PQuery("SELECT  `auction`.`itemguid`, `auction`.`itemowner`, `characters`.`account`, `characters`.`name` "
                                        "FROM `auction`,`characters` WHERE `auction`.`item_template`='%u' AND `characters`.`guid` = `auction`.`itemowner` LIMIT %u",
                                        itemId, count);
    }

    QueryResult* QueryGuildBankCountOf(uint32 itemId)
    {
        return CharacterDatabase.PQuery("SELECT COUNT(`item_entry`) FROM `guild_bank_item` WHERE `item_entry`='%u'", itemId);
    }

    QueryResult* QueryGuildBankHoldersOf(uint32 itemId, uint32 count)
    {
        //                                      0             1           2
        return CharacterDatabase.PQuery("SELECT gi.item_guid, gi.guildid, guild.name "
                                        "FROM guild_bank_item AS gi, guild WHERE gi.item_entry='%u' AND gi.guildid = guild.guildid LIMIT %u ",
                                        itemId, count);
    }
}
