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

/// PlayerDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/players/PlayerDataRepository.h"

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

// Each column of a `playercreateinfo` row lands in its member, and the rows come back in order.
TEST(PlayerDataRepository_CreateInfo)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004.5", "1005.5", "1006.5", "1007.5", "10008"},
        FakeRow{"2000", "2001", "2002", "2003", "2004.5", "2005.5", "2006.5", "2007.5", "20008"},
    });
    std::vector<PlayerCreateInfoRow> rows = PlayerDataRepository::QueryCreateInfo();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].race, 1000u);
    CHECK_EQ(rows[0].classId, 1001u);
    CHECK_EQ(rows[0].map, 1002u);
    CHECK_EQ(rows[0].zone, 1003u);
    CHECK_EQ(rows[0].positionX, 1004.5f);
    CHECK_EQ(rows[0].positionY, 1005.5f);
    CHECK_EQ(rows[0].positionZ, 1006.5f);
    CHECK_EQ(rows[0].orientation, 1007.5f);
    CHECK_EQ(rows[0].phaseMap, uint16(10008));
    CHECK_EQ(rows[1].race, 2000u);
}

// Each column of a `playercreateinfo_item` row lands in its member, and the rows come back in
// order.
TEST(PlayerDataRepository_CreateItems)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    std::vector<PlayerCreateItemRow> rows = PlayerDataRepository::QueryCreateItems();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].race, 1000u);
    CHECK_EQ(rows[0].classId, 1001u);
    CHECK_EQ(rows[0].itemid, 1002u);
    CHECK_EQ(rows[0].amount, 1003u);
    CHECK_EQ(rows[1].race, 2000u);
}

// Each column of a `playercreateinfo_spell` row lands in its member, and the rows come back in
// order.
TEST(PlayerDataRepository_CreateSpells)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002"},
        FakeRow{"2000", "2001", "2002"},
    });
    std::vector<PlayerCreateSpellRow> rows = PlayerDataRepository::QueryCreateSpells();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].race, 1000u);
    CHECK_EQ(rows[0].classId, 1001u);
    CHECK_EQ(rows[0].spell, 1002u);
    CHECK_EQ(rows[1].race, 2000u);
}

// Each column of a `playercreateinfo_action` row lands in its member, and the rows come back in
// order.
TEST(PlayerDataRepository_CreateActions)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "102", "1003", "104"},
        FakeRow{"2000", "2001", "202", "2003", "204"},
    });
    std::vector<PlayerCreateActionRow> rows = PlayerDataRepository::QueryCreateActions();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].race, 1000u);
    CHECK_EQ(rows[0].classId, 1001u);
    CHECK_EQ(rows[0].button, uint8(102));
    CHECK_EQ(rows[0].action, 1003u);
    CHECK_EQ(rows[0].type, uint8(104));
    CHECK_EQ(rows[1].race, 2000u);
}

// Each column of a `player_xp_for_level` row lands in its member, and the rows come back in order.
TEST(PlayerDataRepository_XpForLevel)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<PlayerXpForLevelRow> rows = PlayerDataRepository::QueryXpForLevel();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].lvl, 1000u);
    CHECK_EQ(rows[0].xpForNextLevel, 1001u);
    CHECK_EQ(rows[1].lvl, 2000u);
}

// Each column of a `exploration_basexp` row lands in its member, and the rows come back in order.
TEST(PlayerDataRepository_ExplorationXp)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<ExplorationBaseXpRow> rows = PlayerDataRepository::QueryExplorationXp();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].level, 1000u);
    CHECK_EQ(rows[0].basexp, 1001u);
    CHECK_EQ(rows[1].level, 2000u);
}

// Each column of a `mail_level_reward` row lands in its member, and the rows come back in order.
TEST(PlayerDataRepository_LevelRewardMails)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"100", "1001", "1002", "1003"},
        FakeRow{"200", "2001", "2002", "2003"},
    });
    std::vector<LevelRewardMailRow> rows = PlayerDataRepository::QueryLevelRewardMails();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].level, uint8(100));
    CHECK_EQ(rows[0].raceMask, 1001u);
    CHECK_EQ(rows[0].mailTemplateId, 1002u);
    CHECK_EQ(rows[0].senderEntry, 1003u);
    CHECK_EQ(rows[1].level, uint8(200));
}

// Each column of a `player_levelstats` row lands in its member, and the rows come back in order.
TEST(PlayerDataRepository_LevelStats)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "103", "104", "105", "106", "107"},
        FakeRow{"2000", "2001", "2002", "203", "204", "205", "206", "207"},
    });
    std::vector<PlayerLevelStatsRow> rows = PlayerDataRepository::QueryLevelStats();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].race, 1000u);
    CHECK_EQ(rows[0].classId, 1001u);
    CHECK_EQ(rows[0].level, 1002u);
    CHECK_EQ(rows[0].stats[0], uint8(103));
    CHECK_EQ(rows[0].stats[1], uint8(104));
    CHECK_EQ(rows[0].stats[2], uint8(105));
    CHECK_EQ(rows[0].stats[3], uint8(106));
    CHECK_EQ(rows[0].stats[4], uint8(107));
    CHECK_EQ(rows[1].race, 2000u);
}

// Each row's first column comes back as a value, in order.
TEST(PlayerDataRepository_ReservedNames)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{FakeRow{"s1.0"}, FakeRow{"s2.0"}});
    std::vector<std::string> values = PlayerDataRepository::QueryReservedNames();
    REQUIRE(values.size() == 2);
    CHECK_STR(values[0], "s1.0");
    CHECK_STR(values[1], "s2.0");
}
