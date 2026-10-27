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

/// ConditionRepository's read on the global WorldDatabase backed by fakes: the statement names its
/// columns, every column of a canned row lands in its member (a negative type as negative), and no
/// row answers an empty table or an unreadable one; ObjectMgr::ReloadConditions answers how each
/// read ended and keeps the conditions it holds when the table cannot be read.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "persistence/world/ConditionRepository.h"

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

// Each column of a `conditions` row lands in its member, a negative type included, and the rows come back in order.
TEST(ConditionRepository_Conditions)
{
    Db db;
    db.query.Answer("SELECT `condition_entry`", FakeRows{
        FakeRow{"1000", "-3", "1002", "1003"},
        FakeRow{"2000", "8", "2002", "2003"},
    });
    TableRead<ConditionRow> read = ConditionRepository::QueryConditions();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `condition_entry`, `type`, `value1`, `value2` FROM `conditions`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].entry, 1000u);
    CHECK_EQ(read.rows[0].type, -3);
    CHECK_EQ(read.rows[0].value1, 1002u);
    CHECK_EQ(read.rows[0].value2, 1003u);
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_EQ(read.rows[1].type, 8);
}

// No row and a count of 0 answer an empty table; the count is over the same select.
TEST(ConditionRepository_NoRowsAnswerEmpty)
{
    Db db;
    db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
    TableRead<ConditionRow> read = ConditionRepository::QueryConditions();
    CHECK(read.status == TableReadStatus::Empty);
    REQUIRE(db.query.executed.size() == 2);
    CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `condition_entry`, `type`, `value1`, `value2` FROM `conditions`) AS probe");
}

// No row and no count answer an unreadable table.
TEST(ConditionRepository_FailedQueryAnswersUnreadable)
{
    Db db;
    TableRead<ConditionRow> read = ConditionRepository::QueryConditions();
    CHECK(read.status == TableReadStatus::QueryFailed);
    CHECK(read.Unreadable());
}

// A reload that reads rows fills the conditions and checks them; a NOT of a lower condition passes
// and a NOT of a lower id with no condition is erased. An unreadable table, one that changed during the read
// and an empty one each keep the conditions already loaded.
TEST(ConditionReload_FillsChecksAndKeeps)
{
    {
        Db db;
        db.query.Answer("SELECT `condition_entry`", FakeRows{
            FakeRow{"60001", "0", "0", "0"},
            FakeRow{"60002", "-3", "60001", "0"},
            FakeRow{"60003", "-3", "59999", "0"},
        });
        CHECK(sObjectMgr.ReloadConditions() == TableReadStatus::Loaded);
    }
    PlayerCondition const* plain = sObjectMgr.GetCondition(60001);
    PlayerCondition const* negated = sObjectMgr.GetCondition(60002);
    REQUIRE(plain != NULL);
    REQUIRE(negated != NULL);
    CHECK_EQ(int32(negated->m_condition), int32(CONDITION_NOT));
    CHECK_EQ(negated->m_value1, 60001u);
    CHECK(sObjectMgr.GetCondition(60003) == NULL);

    {
        Db db;
        CHECK(sObjectMgr.ReloadConditions() == TableReadStatus::QueryFailed);
    }
    CHECK(sObjectMgr.GetCondition(60002) != NULL);
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"5"} });
        CHECK(sObjectMgr.ReloadConditions() == TableReadStatus::ChangedWhileRead);
    }
    CHECK(sObjectMgr.GetCondition(60002) != NULL);
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        CHECK(sObjectMgr.ReloadConditions() == TableReadStatus::Empty);
    }
    CHECK(sObjectMgr.GetCondition(60002) != NULL);
}
