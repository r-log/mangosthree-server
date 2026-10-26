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

/// ItemDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/items/ItemDataRepository.h"

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

// Each column of a `item_convert` row lands in its member, and the rows come back in order.
TEST(ItemDataRepository_ItemConverts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<ItemConvertRow> rows = ItemDataRepository::QueryItemConverts();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].item, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `item_expire_convert` row lands in its member, and the rows come back in order.
TEST(ItemDataRepository_ItemExpireConverts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<ItemExpireConvertRow> rows = ItemDataRepository::QueryItemExpireConverts();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].item, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `item_required_target` row lands in its member, and the rows come back in order.
TEST(ItemDataRepository_ItemRequiredTargets)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<ItemRequiredTargetRow> rows = ItemDataRepository::QueryItemRequiredTargets();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].type, 1001u);
    CHECK_EQ(rows[0].targetEntry, 1002u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `hotfix_data` row lands in its member, and the rows come back in order.
TEST(ItemDataRepository_Hotfixes)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<HotfixRow> rows = ItemDataRepository::QueryHotfixes();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].type, 1001u);
    CHECK_EQ(rows[0].timestamp, 1002u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_item` row lands in its member, and the rows come back in order.
TEST(ItemDataRepository_ItemLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22"},
    });
    std::vector<ItemLocaleRow> rows = ItemDataRepository::QueryItemLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].name[0], "s1.1");
    CHECK_STR(rows[0].description[0], "s1.2");
    CHECK_STR(rows[0].name[1], "s1.3");
    CHECK_STR(rows[0].description[1], "s1.4");
    CHECK_STR(rows[0].name[2], "s1.5");
    CHECK_STR(rows[0].description[2], "s1.6");
    CHECK_STR(rows[0].name[3], "s1.7");
    CHECK_STR(rows[0].description[3], "s1.8");
    CHECK_STR(rows[0].name[4], "s1.9");
    CHECK_STR(rows[0].description[4], "s1.10");
    CHECK_STR(rows[0].name[5], "s1.11");
    CHECK_STR(rows[0].description[5], "s1.12");
    CHECK_STR(rows[0].name[6], "s1.13");
    CHECK_STR(rows[0].description[6], "s1.14");
    CHECK_STR(rows[0].name[7], "s1.15");
    CHECK_STR(rows[0].description[7], "s1.16");
    CHECK_STR(rows[0].name[8], "s1.17");
    CHECK_STR(rows[0].description[8], "s1.18");
    CHECK_STR(rows[0].name[9], "s1.19");
    CHECK_STR(rows[0].description[9], "s1.20");
    CHECK_STR(rows[0].name[10], "s1.21");
    CHECK_STR(rows[0].description[10], "s1.22");
    CHECK_EQ(rows[1].entry, 2000u);
}
