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

/// LootDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/economy/LootDataRepository.h"

#include <vector>

namespace
{
    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };
}

// Each column of a row of a loot template table (`creature_loot_template` and the tables like it)
// lands in its member, and the rows come back in order.
TEST(LootDataRepository_LootTemplate)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "-1001", "1002.5", "103", "-1004", "1005", "10006"},
        FakeRow{"2000", "-2001", "2002.5", "203", "-2004", "2005", "20006"},
    });
    std::vector<LootTemplateRow> rows = LootDataRepository::QueryLootTemplate("creature_loot_template");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].item, -1001);
    CHECK_EQ(rows[0].chanceOrQuestChance, 1002.5f);
    CHECK_EQ(rows[0].groupid, uint8(103));
    CHECK_EQ(rows[0].mincountOrRef, -1004);
    CHECK_EQ(rows[0].maxcount, 1005u);
    CHECK_EQ(rows[0].conditionId, uint16(10006));
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `item_enchantment_template` row lands in its member, and the rows come back in
// order.
TEST(LootDataRepository_ItemEnchantments)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"-1000", "1001", "1002.5"},
        FakeRow{"-2000", "2001", "2002.5"},
    });
    std::vector<ItemEnchantmentRow> rows = LootDataRepository::QueryItemEnchantments();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, -1000);
    CHECK_EQ(rows[0].ench, 1001u);
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[1].entry, -2000);
}
