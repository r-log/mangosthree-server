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

/// PoolDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/pools/PoolDataRepository.h"

#include <optional>
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

// The read answers the first column of the first row, and nothing for no rows.
TEST(PoolDataRepository_HighestPoolEntry)
{
    {
        Db db;
        std::optional<uint16> value = PoolDataRepository::QueryHighestPoolEntry();
        CHECK(!value);
    }
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"10007"}, FakeRow{"20007"}});
    std::optional<uint16> value = PoolDataRepository::QueryHighestPoolEntry();
    REQUIRE(value);
    CHECK_EQ(*value, uint16(10007));
}

// Each column of a `pool_template` row lands in its member, and the rows come back in order.
TEST(PoolDataRepository_Pools)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10000", "1001", "s1.2"},
        FakeRow{"20000", "2001", "s2.2"},
    });
    std::vector<PoolTemplateRow> rows = PoolDataRepository::QueryPools();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, uint16(10000));
    CHECK_EQ(rows[0].maxLimit, 1001u);
    CHECK_STR(rows[0].description, "s1.2");
    CHECK_EQ(rows[1].entry, uint16(20000));
}

// Each column of a `pool_creature` row lands in its member, and the rows come back in order.
TEST(PoolDataRepository_PoolCreatures)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002.5"},
        FakeRow{"2000", "20001", "2002.5"},
    });
    std::vector<PoolCreatureRow> rows = PoolDataRepository::QueryPoolCreatures();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].poolEntry, uint16(10001));
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a creature template's pool (`pool_creature_template`), with the guid of each of
// its spawns lands in its member, and the rows come back in order.
TEST(PoolDataRepository_PoolCreatureTemplates)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002.5", "1003"},
        FakeRow{"2000", "20001", "2002.5", "2003"},
    });
    std::vector<PoolCreatureTemplateRow> rows = PoolDataRepository::QueryPoolCreatureTemplates();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].poolEntry, uint16(10001));
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[0].id, 1003u);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a `pool_gameobject` row lands in its member, and the rows come back in order.
TEST(PoolDataRepository_PoolGameObjects)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002.5"},
        FakeRow{"2000", "20001", "2002.5"},
    });
    std::vector<PoolGameObjectRow> rows = PoolDataRepository::QueryPoolGameObjects();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].poolEntry, uint16(10001));
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a game object template's pool (`pool_gameobject_template`), with the guid of each
// of its spawns lands in its member, and the rows come back in order.
TEST(PoolDataRepository_PoolGameObjectTemplates)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002.5", "1003"},
        FakeRow{"2000", "20001", "2002.5", "2003"},
    });
    std::vector<PoolGameObjectTemplateRow> rows = PoolDataRepository::QueryPoolGameObjectTemplates();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].poolEntry, uint16(10001));
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[0].id, 1003u);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a `pool_pool` row lands in its member, and the rows come back in order.
TEST(PoolDataRepository_PoolPools)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10000", "10001", "1002.5"},
        FakeRow{"20000", "20001", "2002.5"},
    });
    std::vector<PoolPoolRow> rows = PoolDataRepository::QueryPoolPools();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].poolId, uint16(10000));
    CHECK_EQ(rows[0].motherPool, uint16(10001));
    CHECK_EQ(rows[0].chance, 1002.5f);
    CHECK_EQ(rows[1].poolId, uint16(20000));
}
