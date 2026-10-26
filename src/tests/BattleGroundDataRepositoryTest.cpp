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

/// BattleGroundDataRepository's row reads on the global WorldDatabase backed by fakes: every column
/// of a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/pvp/BattleGroundDataRepository.h"

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

// Each column of a `battleground_template` row lands in its member, and the rows come back in
// order.
TEST(BattleGroundDataRepository_Templates)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004.5", "1005", "1006.5", "1007.5"},
        FakeRow{"2000", "2001", "2002", "2003", "2004.5", "2005", "2006.5", "2007.5"},
    });
    std::vector<BattleGroundTemplateRow> rows = BattleGroundDataRepository::QueryTemplates();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].minPlayersPerTeam, 1001u);
    CHECK_EQ(rows[0].maxPlayersPerTeam, 1002u);
    CHECK_EQ(rows[0].allianceStartLoc, 1003u);
    CHECK_EQ(rows[0].allianceStartO, 1004.5f);
    CHECK_EQ(rows[0].hordeStartLoc, 1005u);
    CHECK_EQ(rows[0].hordeStartO, 1006.5f);
    CHECK_EQ(rows[0].startMaxDist, 1007.5f);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `battlemaster_entry` row lands in its member, and the rows come back in order.
TEST(BattleGroundDataRepository_BattleMasters)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<BattleMasterRow> rows = BattleGroundDataRepository::QueryBattleMasters();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].bgTemplate, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a battleground event of a spawn lands in its member, and the rows come back in
// order.
TEST(BattleGroundDataRepository_EventIndexes)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"100", "1001", "102", "103", "1004", "1005", "1006", "107", "108", "s1.9"},
        FakeRow{"200", "2001", "202", "203", "2004", "2005", "2006", "207", "208"},
    });
    std::vector<BattleGroundEventIndexRow> rows = BattleGroundDataRepository::QueryEventIndexes();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].type, uint8(100));
    CHECK_EQ(rows[0].guid, 1001u);
    CHECK_EQ(rows[0].event1, uint8(102));
    CHECK_EQ(rows[0].event2, uint8(103));
    CHECK_EQ(rows[0].map, 1004u);
    CHECK_EQ(rows[0].spawnGuid, 1005u);
    CHECK_EQ(rows[0].descriptionMap, 1006u);
    CHECK_EQ(rows[0].descriptionEvent1, uint8(107));
    CHECK_EQ(rows[0].descriptionEvent2, uint8(108));
    REQUIRE(rows[0].description);
    CHECK_STR(*rows[0].description, "s1.9");
    CHECK_EQ(rows[1].type, uint8(200));
    CHECK(!rows[1].description);
}
