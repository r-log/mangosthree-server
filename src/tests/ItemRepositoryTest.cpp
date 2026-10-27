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

/// The items' repository on the global CharacterDatabase backed by fakes: every statement an item
/// writes, in the order it is called and with its values substituted as the server would send
/// them to MySQL after ExecuteQueuedForTest().

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/inventory/ItemRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// A new item's save, a changed wrapped item's save and a removed item's delete, as SaveToDB issues
// them.
TEST(ItemRepository_InstanceWrites)
{
    Db db;
    {
        TickGuard::Scope scope;
        ItemRepository::DeleteInstance(55);
        ItemRepository::InsertInstance(55, 7, "1 2 3 ", "a note");
        ItemRepository::UpdateInstance(55, 8, "1 2 4 ", "");
        ItemRepository::SetGiftOwner(55, 8);
        ItemRepository::UpdateInstanceData(55, 8, "1 2 5 ");
        ItemRepository::DeleteGift(55);
        ItemRepository::DeleteInventoryRow(55);
        CHECK_EQ(TickGuard::Violations(), 0u);
    }
    CheckLines(db.Take(), {
        "DELETE FROM `item_instance` WHERE `guid` = '55'",
        "INSERT INTO `item_instance` (`guid`,`owner_guid`,`data`,`text`) VALUES ('55', '7', '1 2 3 ', 'a note')",
        "UPDATE `item_instance` SET `data` = '1 2 4 ', `owner_guid` = '8', `text` = '' WHERE `guid` = '55'",
        "UPDATE `character_gifts` SET `guid` = '8' WHERE `item_guid` = '55'",
        "UPDATE `item_instance` SET `data` = '1 2 5 ', `owner_guid` = '8' WHERE `guid` = '55'",
        "DELETE FROM `character_gifts` WHERE `item_guid` = '55'",
        "DELETE FROM `character_inventory` WHERE `item` = '55'",
    }, __LINE__);
    CHECK_EQ(db.query.executed.size(), size_t(0));
}

// The saved loot: the old rows deleted, the money row, then each item and currency row through one
// statement in the order handed; and the load's two deletes of an unknown item or currency.
TEST(ItemRepository_LootWrites)
{
    Db db;
    ItemLootRow item;
    item.itemId = 2589;
    item.count = 3;
    item.randomSuffix = 0;
    item.randomPropertyId = -12;
    ItemLootRow currency;
    currency.itemId = -395;
    currency.count = 10;
    ItemRepository::DeleteLoot(55);
    ItemRepository::InsertLootMoney(55, 7, 1250);
    ItemRepository::InsertLootItems(55, 7, { item, currency });
    ItemRepository::InsertLootItems(56, 7, {});
    CheckLines(db.Take(), {
        "DELETE FROM `item_loot` WHERE `guid` = '55'",
        "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`suffix`,`property`) VALUES ('55', '7', 0, '1250', 0, 0)",
        "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`suffix`,`property`) VALUES ('55', '7', '2589', '3', '0', '-12')",
        "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`suffix`,`property`) VALUES ('55', '7', '-395', '10', '0', '0')",
    }, __LINE__);
}

// A wrapped item's gift read is queued and answered on the world thread; wrapping inserts the row
// inside the transaction the wrap opens, and unwrapping deletes it.
TEST(ItemRepository_GiftRows)
{
    Db db;
    int answered = 0;
    {
        TickGuard::Scope scope;
        ItemRepository::QueryGift(9, [&answered](std::optional<GiftRow> /*gift*/) { ++answered; });
        CharacterTransaction::Begin();
        ItemRepository::InsertGift(42, 9, 5043, 8);
        CharacterTransaction::Commit();
        ItemRepository::DeleteGift(9);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "SELECT `entry`, `flags` FROM `character_gifts` WHERE `item_guid` = '9'",
        "INSERT INTO `character_gifts` VALUES ('42', '9', '5043', '8')",
        "DELETE FROM `character_gifts` WHERE `item_guid` = '9'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 1);
}

// The gift read answers the item's entry and flags, and nothing when the item has no gift row.
TEST(ItemRepository_GiftRow)
{
    {
        Db db;
        db.async.Answer("SELECT `entry`, `flags`", FakeRows{FakeRow{"1000", "1001"}});
        std::optional<GiftRow> gift;
        ItemRepository::QueryGift(9, [&gift](std::optional<GiftRow> got) { gift = got; });
        db.Take();
        CharacterDatabase.ProcessResultQueue();
        REQUIRE(gift);
        CHECK_EQ(gift->entry, 1000u);
        CHECK_EQ(gift->flags, 1001u);
    }
    Db db;
    bool answered = false;
    std::optional<GiftRow> gift;
    ItemRepository::QueryGift(9, [&answered, &gift](std::optional<GiftRow> got) { answered = true; gift = got; });
    db.Take();
    CharacterDatabase.ProcessResultQueue();
    CHECK(answered);
    CHECK(!gift);
}
