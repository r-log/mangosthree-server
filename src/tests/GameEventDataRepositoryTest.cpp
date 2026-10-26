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

/// GameEventDataRepository's row reads on the global WorldDatabase backed by fakes: every column of
/// a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/events/GameEventDataRepository.h"

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
TEST(GameEventDataRepository_HighestEventEntry)
{
    {
        Db db;
        std::optional<uint16> value = GameEventDataRepository::QueryHighestEventEntry();
        CHECK(!value);
    }
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"10007"}, FakeRow{"20007"}});
    std::optional<uint16> value = GameEventDataRepository::QueryHighestEventEntry();
    REQUIRE(value);
    CHECK_EQ(*value, uint16(10007));
}

// Each column of a `game_event` row lands in its member, and the rows come back in order.
TEST(GameEventDataRepository_Events)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10000", "4294967297", "4294967298", "1003", "1004", "1005", "s1.6"},
        FakeRow{"20000", "8589934593", "8589934594", "2003", "2004", "2005", "s2.6"},
    });
    std::vector<GameEventRow> rows = GameEventDataRepository::QueryEvents();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, uint16(10000));
    CHECK_EQ(rows[0].startTime, uint64(4294967297ull));
    CHECK_EQ(rows[0].endTime, uint64(4294967298ull));
    CHECK_EQ(rows[0].occurence, 1003u);
    CHECK_EQ(rows[0].length, 1004u);
    CHECK_EQ(rows[0].holiday, 1005u);
    CHECK_STR(rows[0].description, "s1.6");
    CHECK_EQ(rows[1].entry, uint16(20000));
}

// Each column of a creature spawn's game event (`game_event_creature`) lands in its member, and the
// rows come back in order.
TEST(GameEventDataRepository_EventCreatures)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "-1001"},
        FakeRow{"2000", "-2001"},
    });
    std::vector<GameEventCreatureRow> rows = GameEventDataRepository::QueryEventCreatures();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].event, int16(-1001));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a game object spawn's game event (`game_event_gameobject`) lands in its member,
// and the rows come back in order.
TEST(GameEventDataRepository_EventGameObjects)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "-1001"},
        FakeRow{"2000", "-2001"},
    });
    std::vector<GameEventGameObjectRow> rows = GameEventDataRepository::QueryEventGameObjects();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].event, int16(-1001));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a creature spawn's changes while a game event runs (`game_event_creature_data`)
// lands in its member, and the rows come back in order.
TEST(GameEventDataRepository_EventCreatureData)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002", "1003", "1004", "1005", "1006"},
        FakeRow{"2000", "20001", "2002", "2003", "2004", "2005", "2006"},
    });
    std::vector<GameEventCreatureDataRow> rows = GameEventDataRepository::QueryEventCreatureData();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].event, uint16(10001));
    CHECK_EQ(rows[0].modelid, 1002u);
    CHECK_EQ(rows[0].equipmentId, 1003u);
    CHECK_EQ(rows[0].entryId, 1004u);
    CHECK_EQ(rows[0].spellStart, 1005u);
    CHECK_EQ(rows[0].spellEnd, 1006u);
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a `game_event_quest` row lands in its member, and the rows come back in order.
TEST(GameEventDataRepository_EventQuests)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001"},
        FakeRow{"2000", "20001"},
    });
    std::vector<GameEventQuestRow> rows = GameEventDataRepository::QueryEventQuests();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].quest, 1000u);
    CHECK_EQ(rows[0].event, uint16(10001));
    CHECK_EQ(rows[1].quest, 2000u);
}

// Each column of a `game_event_mail` row lands in its member, and the rows come back in order.
TEST(GameEventDataRepository_EventMails)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10000", "1001", "1002", "1003", "1004"},
        FakeRow{"20000", "2001", "2002", "2003", "2004"},
    });
    std::vector<GameEventMailRow> rows = GameEventDataRepository::QueryEventMails();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].event, uint16(10000));
    CHECK_EQ(rows[0].raceMask, 1001u);
    CHECK_EQ(rows[0].quest, 1002u);
    CHECK_EQ(rows[0].mailTemplateId, 1003u);
    CHECK_EQ(rows[0].senderEntry, 1004u);
    CHECK_EQ(rows[1].event, uint16(20000));
}
