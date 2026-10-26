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

/// CreatureEventAIRepository's row reads on the global WorldDatabase backed by fakes: every column
/// of a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/ai/CreatureEventAIRepository.h"

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

// Each column of a `creature_ai_summons` row lands in its member, and the rows come back in order.
TEST(CreatureEventAIRepository_Summons)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "1004.5", "1005"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "2004.5", "2005"},
    });
    std::vector<CreatureAISummonRow> rows = CreatureEventAIRepository::QuerySummons();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].orientation, 1004.5f);
    CHECK_EQ(rows[0].spawntimesecs, 1005u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `creature_ai_scripts` row lands in its member, and the rows come back in order.
TEST(CreatureEventAIRepository_Events)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "104", "105", "1006", "1007", "1008", "1009",
            "10010", "1011", "1012", "1013", "10014", "1015", "1016", "1017", "10018", "1019",
            "1020", "1021"},
        FakeRow{"2000", "2001", "2002", "2003", "204", "205", "2006", "2007", "2008", "2009",
            "20010", "2011", "2012", "2013", "20014", "2015", "2016", "2017", "20018", "2019",
            "2020", "2021"},
    });
    std::vector<CreatureAIEventRow> rows = CreatureEventAIRepository::QueryEvents();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].creatureId, 1001u);
    CHECK_EQ(rows[0].eventType, 1002u);
    CHECK_EQ(rows[0].eventInversePhaseMask, 1003u);
    CHECK_EQ(rows[0].eventChance, uint8(104));
    CHECK_EQ(rows[0].eventFlags, uint8(105));
    CHECK_EQ(rows[0].eventParam1, 1006u);
    CHECK_EQ(rows[0].eventParam2, 1007u);
    CHECK_EQ(rows[0].eventParam3, 1008u);
    CHECK_EQ(rows[0].eventParam4, 1009u);
    CHECK_EQ(rows[0].actions[0].type, uint16(10010));
    CHECK_EQ(rows[0].actions[0].param1, 1011u);
    CHECK_EQ(rows[0].actions[0].param2, 1012u);
    CHECK_EQ(rows[0].actions[0].param3, 1013u);
    CHECK_EQ(rows[0].actions[1].type, uint16(10014));
    CHECK_EQ(rows[0].actions[1].param1, 1015u);
    CHECK_EQ(rows[0].actions[1].param2, 1016u);
    CHECK_EQ(rows[0].actions[1].param3, 1017u);
    CHECK_EQ(rows[0].actions[2].type, uint16(10018));
    CHECK_EQ(rows[0].actions[2].param1, 1019u);
    CHECK_EQ(rows[0].actions[2].param2, 1020u);
    CHECK_EQ(rows[0].actions[2].param3, 1021u);
    CHECK_EQ(rows[1].id, 2000u);
}
