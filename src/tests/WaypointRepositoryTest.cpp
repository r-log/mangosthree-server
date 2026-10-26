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

/// WaypointRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/waypoints/WaypointRepository.h"

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

// Each column of The number of points of one creature's path (`creature_movement`) lands in its
// member, and the rows come back in order.
TEST(WaypointRepository_PathPointCounts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<PathPointCountRow> rows = WaypointRepository::QueryPathPointCounts();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].count, 1001u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `creature_movement` row lands in its member, and the rows come back in order.
TEST(WaypointRepository_PathPoints)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005", "1006", "-1007", "-1008",
            "-1009", "-1010", "-1011", "1012", "1013", "1014.5", "1015", "1016"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005", "2006", "-2007", "-2008",
            "-2009", "-2010", "-2011", "2012", "2013", "2014.5", "2015", "2016"},
    });
    std::vector<PathPointRow> rows = WaypointRepository::QueryPathPoints();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].point, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].waittime, 1005u);
    CHECK_EQ(rows[0].scriptId, 1006u);
    CHECK_EQ(rows[0].textid[0], -1007);
    CHECK_EQ(rows[0].textid[1], -1008);
    CHECK_EQ(rows[0].textid[2], -1009);
    CHECK_EQ(rows[0].textid[3], -1010);
    CHECK_EQ(rows[0].textid[4], -1011);
    CHECK_EQ(rows[0].emote, 1012u);
    CHECK_EQ(rows[0].spell, 1013u);
    CHECK_EQ(rows[0].orientation, 1014.5f);
    CHECK_EQ(rows[0].model1, 1015u);
    CHECK_EQ(rows[0].model2, 1016u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of The number of points of one creature template's path
// (`creature_movement_template`) lands in its member, and the rows come back in order.
TEST(WaypointRepository_TemplatePathPointCounts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<TemplatePathPointCountRow> rows = WaypointRepository::QueryTemplatePathPointCounts();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].count, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `creature_movement_template` row lands in its member, and the rows come back in
// order.
TEST(WaypointRepository_TemplatePathPoints)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005", "1006", "1007", "1008",
            "1009", "1010", "1011", "1012", "1013", "1014.5", "1015", "1016"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005", "2006", "2007", "2008",
            "2009", "2010", "2011", "2012", "2013", "2014.5", "2015", "2016"},
    });
    std::vector<TemplatePathPointRow> rows = WaypointRepository::QueryTemplatePathPoints();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].point, 1001u);
    CHECK_EQ(rows[0].positionX, 1002.5f);
    CHECK_EQ(rows[0].positionY, 1003.5f);
    CHECK_EQ(rows[0].positionZ, 1004.5f);
    CHECK_EQ(rows[0].waittime, 1005u);
    CHECK_EQ(rows[0].scriptId, 1006u);
    CHECK_EQ(rows[0].textid[0], 1007u);
    CHECK_EQ(rows[0].textid[1], 1008u);
    CHECK_EQ(rows[0].textid[2], 1009u);
    CHECK_EQ(rows[0].textid[3], 1010u);
    CHECK_EQ(rows[0].textid[4], 1011u);
    CHECK_EQ(rows[0].emote, 1012u);
    CHECK_EQ(rows[0].spell, 1013u);
    CHECK_EQ(rows[0].orientation, 1014.5f);
    CHECK_EQ(rows[0].model1, 1015u);
    CHECK_EQ(rows[0].model2, 1016u);
    CHECK_EQ(rows[1].entry, 2000u);
}
