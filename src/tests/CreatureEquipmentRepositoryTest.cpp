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

/// CreatureEquipmentRepository's read on the global WorldDatabase backed by fakes: the statement
/// names its columns, every column of a canned row lands in its member, and no row answers an
/// empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/creatures/CreatureEquipmentRepository.h"

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

// Each column of a `creature_equip_template` row lands in its member, and the rows come back in order.
TEST(CreatureEquipmentRepository_EquipmentTemplates)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    TableRead<CreatureEquipmentRow> read = CreatureEquipmentRepository::QueryEquipmentTemplates();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `equipentry1`, `equipentry2`, `equipentry3` FROM `creature_equip_template`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].entry, 1000u);
    CHECK_EQ(read.rows[0].equipentry1, 1001u);
    CHECK_EQ(read.rows[0].equipentry2, 1002u);
    CHECK_EQ(read.rows[0].equipentry3, 1003u);
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_EQ(read.rows[1].equipentry3, 2003u);
}

// No row and a count of 0 answer an empty table; the count is over the same select.
TEST(CreatureEquipmentRepository_NoRowsAnswerEmpty)
{
    Db db;
    db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
    TableRead<CreatureEquipmentRow> read = CreatureEquipmentRepository::QueryEquipmentTemplates();
    CHECK(read.status == TableReadStatus::Empty);
    REQUIRE(db.query.executed.size() == 2);
    CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `entry`, `equipentry1`, `equipentry2`, `equipentry3` FROM `creature_equip_template`) AS probe");
}

// No row and no count answer an unreadable table.
TEST(CreatureEquipmentRepository_FailedQueryAnswersUnreadable)
{
    Db db;
    TableRead<CreatureEquipmentRow> read = CreatureEquipmentRepository::QueryEquipmentTemplates();
    CHECK(read.status == TableReadStatus::QueryFailed);
    CHECK(read.Unreadable());
}
