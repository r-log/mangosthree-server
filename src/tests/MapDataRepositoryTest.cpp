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

/// MapDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/maps/MapDataRepository.h"

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

// Each column of a `game_tele` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_Teles)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "1004.5", "1005", "s1.6"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "2004.5", "2005", "s2.6"},
    });
    std::vector<GameTeleRow> rows = MapDataRepository::QueryTeles();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].positionX, 1001.5f);
    CHECK_EQ(rows[0].positionY, 1002.5f);
    CHECK_EQ(rows[0].positionZ, 1003.5f);
    CHECK_EQ(rows[0].orientation, 1004.5f);
    CHECK_EQ(rows[0].map, 1005u);
    CHECK_STR(rows[0].name, "s1.6");
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `game_graveyard_zone` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_GraveyardZones)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<GraveyardZoneRow> rows = MapDataRepository::QueryGraveyardZones();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].ghostZone, 1001u);
    CHECK_EQ(rows[0].faction, 1002u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `areatrigger_teleport` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_AreaTriggerTeleports)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "1004", "1005", "1006", "1007", "1008", "1009.5",
            "1010.5", "1011.5", "1012.5"},
        FakeRow{"2000", "201", "2002", "2003", "2004", "2005", "2006", "2007", "2008", "2009.5",
            "2010.5", "2011.5", "2012.5"},
    });
    std::vector<AreaTriggerTeleportRow> rows = MapDataRepository::QueryAreaTriggerTeleports();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].requiredLevel, uint8(101));
    CHECK_EQ(rows[0].requiredItem, 1002u);
    CHECK_EQ(rows[0].requiredItem2, 1003u);
    CHECK_EQ(rows[0].heroicKey, 1004u);
    CHECK_EQ(rows[0].heroicKey2, 1005u);
    CHECK_EQ(rows[0].requiredQuestDone, 1006u);
    CHECK_EQ(rows[0].requiredQuestDoneHeroic, 1007u);
    CHECK_EQ(rows[0].targetMap, 1008u);
    CHECK_EQ(rows[0].targetPositionX, 1009.5f);
    CHECK_EQ(rows[0].targetPositionY, 1010.5f);
    CHECK_EQ(rows[0].targetPositionZ, 1011.5f);
    CHECK_EQ(rows[0].targetOrientation, 1012.5f);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `areatrigger_tavern` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_TavernAreaTriggers)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000"},
        FakeRow{"2000"},
    });
    std::vector<TavernAreaTriggerRow> rows = MapDataRepository::QueryTavernAreaTriggers();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `transports` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_Transports)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "1002"},
        FakeRow{"2000", "s2.1", "2002"},
    });
    std::vector<TransportRow> rows = MapDataRepository::QueryTransports();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].name, "s1.1");
    CHECK_EQ(rows[0].period, 1002u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a transport game object spawn, with its transport's name lands in its member, and
// the rows come back in order.
TEST(MapDataRepository_TransportGameObjects)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "s1.2"},
        FakeRow{"2000", "2001", "s2.2"},
    });
    std::vector<TransportGameObjectRow> rows = MapDataRepository::QueryTransportGameObjects();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].id, 1001u);
    CHECK_STR(rows[0].name, "s1.2");
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a `game_weather` row lands in its member, and the rows come back in order.
TEST(MapDataRepository_Weather)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007", "1008", "1009",
            "1010", "1011", "1012"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005", "2006", "2007", "2008", "2009",
            "2010", "2011", "2012"},
    });
    std::vector<WeatherRow> rows = MapDataRepository::QueryWeather();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].zone, 1000u);
    CHECK_EQ(rows[0].rainChance[0], 1001u);
    CHECK_EQ(rows[0].snowChance[0], 1002u);
    CHECK_EQ(rows[0].stormChance[0], 1003u);
    CHECK_EQ(rows[0].rainChance[1], 1004u);
    CHECK_EQ(rows[0].snowChance[1], 1005u);
    CHECK_EQ(rows[0].stormChance[1], 1006u);
    CHECK_EQ(rows[0].rainChance[2], 1007u);
    CHECK_EQ(rows[0].snowChance[2], 1008u);
    CHECK_EQ(rows[0].stormChance[2], 1009u);
    CHECK_EQ(rows[0].rainChance[3], 1010u);
    CHECK_EQ(rows[0].snowChance[3], 1011u);
    CHECK_EQ(rows[0].stormChance[3], 1012u);
    CHECK_EQ(rows[1].zone, 2000u);
}
