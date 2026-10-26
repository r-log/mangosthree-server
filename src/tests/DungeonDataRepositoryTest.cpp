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

/// DungeonDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/maps/DungeonDataRepository.h"

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

// Each column of a `instance_encounters` row lands in its member, and the rows come back in order.
TEST(DungeonDataRepository_InstanceEncounters)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "1003"},
        FakeRow{"2000", "201", "2002", "2003"},
    });
    std::vector<InstanceEncounterRow> rows = DungeonDataRepository::QueryInstanceEncounters();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].creditType, uint8(101));
    CHECK_EQ(rows[0].creditEntry, 1002u);
    CHECK_EQ(rows[0].lastEncounterDungeon, 1003u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `dungeonfinder_requirements` row lands in its member, and the rows come back in
// order.
TEST(DungeonDataRepository_DungeonFinderRequirements)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007", "s1.8"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005", "2006", "2007"},
    });
    std::vector<DungeonFinderRequirementRow> rows = DungeonDataRepository::QueryDungeonFinderRequirements();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].mapId, 1000u);
    CHECK_EQ(rows[0].difficulty, 1001u);
    CHECK_EQ(rows[0].minItemLevel, 1002u);
    CHECK_EQ(rows[0].item, 1003u);
    CHECK_EQ(rows[0].item2, 1004u);
    CHECK_EQ(rows[0].allianceQuest, 1005u);
    CHECK_EQ(rows[0].hordeQuest, 1006u);
    CHECK_EQ(rows[0].achievement, 1007u);
    REQUIRE(rows[0].questIncompleteText);
    CHECK_STR(*rows[0].questIncompleteText, "s1.8");
    CHECK_EQ(rows[1].mapId, 2000u);
    CHECK(!rows[1].questIncompleteText);
}

// Each column of a `dungeonfinder_rewards` row lands in its member, and the rows come back in
// order.
TEST(DungeonDataRepository_DungeonFinderRewards)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"0", "1001", "1002", "-1003"},
        FakeRow{"0", "2001", "2002", "-2003"},
    });
    std::vector<DungeonFinderRewardRow> rows = DungeonDataRepository::QueryDungeonFinderRewards();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].level, 1001u);
    CHECK_EQ(rows[0].baseXpReward, 1002u);
    CHECK_EQ(rows[0].baseMonetaryReward, -1003);
    CHECK_EQ(rows[1].level, 2001u);
}

// Each column of a `dungeonfinder_item_rewards` row lands in its member, and the rows come back in
// order.
TEST(DungeonDataRepository_DungeonFinderItemRewards)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "-1003", "1004", "1005"},
        FakeRow{"2000", "2001", "2002", "-2003", "2004", "2005"},
    });
    std::vector<DungeonFinderItemRewardRow> rows = DungeonDataRepository::QueryDungeonFinderItemRewards();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].minLevel, 1001u);
    CHECK_EQ(rows[0].maxLevel, 1002u);
    CHECK_EQ(rows[0].itemReward, -1003);
    CHECK_EQ(rows[0].itemAmount, 1004u);
    CHECK_EQ(rows[0].dungeonType, 1005u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `lfg_dungeon_entrances` row lands in its member, and the rows come back in
// order.
TEST(DungeonDataRepository_DungeonEntrances)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "1004.5"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "2004.5"},
    });
    std::vector<DungeonEntranceRow> rows = DungeonDataRepository::QueryDungeonEntrances();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].dungeonId, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].orientation, 1004.5f);
    CHECK_EQ(rows[1].dungeonId, 2000u);
}
