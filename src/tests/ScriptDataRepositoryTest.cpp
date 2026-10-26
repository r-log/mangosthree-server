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

/// ScriptDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/ScriptDataRepository.h"

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

// Each column of a `script_binding` row lands in its member, and the rows come back in order.
TEST(ScriptDataRepository_ScriptBindings)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"100", "-1001", "s1.2", "103"},
        FakeRow{"200", "-2001", "s2.2", "203"},
    });
    std::vector<ScriptBindingRow> rows = ScriptDataRepository::QueryScriptBindings();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].type, uint8(100));
    CHECK_EQ(rows[0].bind, -1001);
    CHECK_STR(rows[0].scriptName, "s1.2");
    CHECK_EQ(rows[0].data, uint8(103));
    CHECK_EQ(rows[1].type, uint8(200));
}

// Each column of a `script_binding` row lands in its member, and the rows come back in order.
TEST(ScriptDataRepository_ScriptNames)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0"},
        FakeRow{},
    });
    std::vector<ScriptNameRow> rows = ScriptDataRepository::QueryScriptNames();
    REQUIRE(rows.size() == 2);
    REQUIRE(rows[0].scriptName);
    CHECK_STR(*rows[0].scriptName, "s1.0");
    CHECK(!rows[1].scriptName);
}

// Each column of a `db_scripts` row lands in its member, and the rows come back in order.
TEST(ScriptDataRepository_DbScripts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "107", "-1008", "-1009",
            "-1010", "-1011", "1012.5", "1013.5", "1014.5", "1015.5"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005", "2006", "207", "-2008", "-2009",
            "-2010", "-2011", "2012.5", "2013.5", "2014.5", "2015.5"},
    });
    std::vector<DbScriptRow> rows = ScriptDataRepository::QueryDbScripts(2);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].delay, 1001u);
    CHECK_EQ(rows[0].command, 1002u);
    CHECK_EQ(rows[0].datalong, 1003u);
    CHECK_EQ(rows[0].datalong2, 1004u);
    CHECK_EQ(rows[0].buddyEntry, 1005u);
    CHECK_EQ(rows[0].searchRadius, 1006u);
    CHECK_EQ(rows[0].dataFlags, uint8(107));
    CHECK_EQ(rows[0].dataint, -1008);
    CHECK_EQ(rows[0].dataint2, -1009);
    CHECK_EQ(rows[0].dataint3, -1010);
    CHECK_EQ(rows[0].dataint4, -1011);
    CHECK_EQ(rows[0].x, 1012.5f);
    CHECK_EQ(rows[0].y, 1013.5f);
    CHECK_EQ(rows[0].z, 1014.5f);
    CHECK_EQ(rows[0].o, 1015.5f);
    CHECK_EQ(rows[1].id, 2000u);
}
