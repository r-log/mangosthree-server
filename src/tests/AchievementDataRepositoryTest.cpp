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

/// AchievementDataRepository's row reads on the global WorldDatabase backed by fakes: every column
/// of a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/achievements/AchievementDataRepository.h"

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

// Each column of a `achievement_criteria_requirement` row lands in its member, and the rows come
// back in order.
TEST(AchievementDataRepository_CriteriaRequirements)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    std::vector<AchievementCriteriaRequirementRow> rows = AchievementDataRepository::QueryCriteriaRequirements();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].criteriaId, 1000u);
    CHECK_EQ(rows[0].type, 1001u);
    CHECK_EQ(rows[0].value1, 1002u);
    CHECK_EQ(rows[0].value2, 1003u);
    CHECK_EQ(rows[1].criteriaId, 2000u);
}

// Each column of a `achievement_reward` row lands in its member, and the rows come back in order.
TEST(AchievementDataRepository_Rewards)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "1002", "1003", "1004", "1005", "s1.6", "s1.7"},
        FakeRow{"2000", "201", "2002", "2003", "2004", "2005", "s2.6", "s2.7"},
    });
    std::vector<AchievementRewardRow> rows = AchievementDataRepository::QueryRewards();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].gender, uint8(101));
    CHECK_EQ(rows[0].titleA, 1002u);
    CHECK_EQ(rows[0].titleH, 1003u);
    CHECK_EQ(rows[0].item, 1004u);
    CHECK_EQ(rows[0].sender, 1005u);
    CHECK_STR(rows[0].subject, "s1.6");
    CHECK_STR(rows[0].text, "s1.7");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_achievement_reward` row lands in its member, and the rows come back in
// order.
TEST(AchievementDataRepository_RewardLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22", "s1.23"},
        FakeRow{"2000", "201", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22", "s2.23"},
    });
    std::vector<AchievementRewardLocaleRow> rows = AchievementDataRepository::QueryRewardLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].gender, uint8(101));
    CHECK_STR(rows[0].subject[0], "s1.2");
    CHECK_STR(rows[0].text[0], "s1.3");
    CHECK_STR(rows[0].subject[1], "s1.4");
    CHECK_STR(rows[0].text[1], "s1.5");
    CHECK_STR(rows[0].subject[2], "s1.6");
    CHECK_STR(rows[0].text[2], "s1.7");
    CHECK_STR(rows[0].subject[3], "s1.8");
    CHECK_STR(rows[0].text[3], "s1.9");
    CHECK_STR(rows[0].subject[4], "s1.10");
    CHECK_STR(rows[0].text[4], "s1.11");
    CHECK_STR(rows[0].subject[5], "s1.12");
    CHECK_STR(rows[0].text[5], "s1.13");
    CHECK_STR(rows[0].subject[6], "s1.14");
    CHECK_STR(rows[0].text[6], "s1.15");
    CHECK_STR(rows[0].subject[7], "s1.16");
    CHECK_STR(rows[0].text[7], "s1.17");
    CHECK_STR(rows[0].subject[8], "s1.18");
    CHECK_STR(rows[0].text[8], "s1.19");
    CHECK_STR(rows[0].subject[9], "s1.20");
    CHECK_STR(rows[0].text[9], "s1.21");
    CHECK_STR(rows[0].subject[10], "s1.22");
    CHECK_STR(rows[0].text[10], "s1.23");
    CHECK_EQ(rows[1].entry, 2000u);
}
