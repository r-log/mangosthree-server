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

/// CreatureModelInfoRepository's read on the global WorldDatabase backed by fakes: the statement
/// names its columns, every column of a canned row lands in its member, and no row answers an
/// empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/creatures/CreatureModelInfoRepository.h"

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

// Each column of a `creature_model_info` row lands in its member, and the rows come back in order.
TEST(CreatureModelInfoRepository_CreatureModelInfos)
{
    Db db;
    db.query.Answer("SELECT `modelid`", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1", "1004", "1005"},
        FakeRow{"2000", "2001.5", "2002.5", "0", "2004", "2005"},
    });
    TableRead<CreatureModelInfoRow> read = CreatureModelInfoRepository::QueryCreatureModelInfos();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `modelid`, `bounding_radius`, `combat_reach`, `gender`, `modelid_other_gender`, `modelid_alternative` FROM `creature_model_info`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].modelid, 1000u);
    CHECK_EQ(read.rows[0].boundingRadius, 1001.5f);
    CHECK_EQ(read.rows[0].combatReach, 1002.5f);
    CHECK_EQ(uint32(read.rows[0].gender), 1u);
    CHECK_EQ(read.rows[0].modelidOtherGender, 1004u);
    CHECK_EQ(read.rows[0].modelidAlternative, 1005u);
    CHECK_EQ(read.rows[1].modelid, 2000u);
    CHECK_EQ(uint32(read.rows[1].gender), 0u);
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(CreatureModelInfoRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<CreatureModelInfoRow> read = CreatureModelInfoRepository::QueryCreatureModelInfos();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `modelid`, `bounding_radius`, `combat_reach`, `gender`, `modelid_other_gender`, `modelid_alternative` FROM `creature_model_info`) AS probe");
    }
    {
        Db db;
        TableRead<CreatureModelInfoRow> read = CreatureModelInfoRepository::QueryCreatureModelInfos();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
