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

/// CreatureDataRepository's row reads on the global WorldDatabase backed by fakes: every column of
/// a canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/creatures/CreatureDataRepository.h"

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

// Each column of a `creature_model_race` row lands in its member, and the rows come back in order.
TEST(CreatureDataRepository_ModelRaces)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    std::vector<CreatureModelRaceRow> rows = CreatureDataRepository::QueryModelRaces();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].modelid, 1000u);
    CHECK_EQ(rows[0].racemask, 1001u);
    CHECK_EQ(rows[0].creatureEntry, 1002u);
    CHECK_EQ(rows[0].modelidRacial, 1003u);
    CHECK_EQ(rows[1].modelid, 2000u);
}

// Each column of a `pet_name_generation` row lands in its member, and the rows come back in order.
TEST(CreatureDataRepository_PetNameParts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"s1.0", "1001", "1"},
        FakeRow{"s2.0", "2001", "1"},
    });
    std::vector<PetNamePartRow> rows = CreatureDataRepository::QueryPetNameParts();
    REQUIRE(rows.size() == 2);
    CHECK_STR(rows[0].word, "s1.0");
    CHECK_EQ(rows[0].entry, 1001u);
    CHECK_EQ(rows[0].half, true);
    CHECK_STR(rows[1].word, "s2.0");
}

// Each column of a `creature_template_classlevelstats` row lands in its member, and the rows come
// back in order.
TEST(CreatureDataRepository_ClassLevelStats)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003.5", "1004.5", "1005", "1006", "1007.5", "1008",
            "1009.5"},
        FakeRow{"2000", "2001", "2002", "2003.5", "2004.5", "2005", "2006", "2007.5", "2008",
            "2009.5"},
    });
    std::vector<ClassLevelStatsRow> rows = CreatureDataRepository::QueryClassLevelStats(1);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].classId, 1000u);
    CHECK_EQ(rows[0].level, 1001u);
    CHECK_EQ(rows[0].baseMana, 1002u);
    CHECK_EQ(rows[0].baseMeleeAttackPower, 1003.5f);
    CHECK_EQ(rows[0].baseRangedAttackPower, 1004.5f);
    CHECK_EQ(rows[0].baseArmor, 1005u);
    CHECK_EQ(rows[0].baseHealth[0], 1006u);
    CHECK_EQ(rows[0].baseDamage[0], 1007.5f);
    CHECK_EQ(rows[0].baseHealth[1], 1008u);
    CHECK_EQ(rows[0].baseDamage[1], 1009.5f);
    CHECK_EQ(rows[1].classId, 2000u);
}

// Each column of a `pet_levelstats` row lands in its member, and the rows come back in order.
TEST(CreatureDataRepository_PetLevelStats)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "10002", "10003", "10004", "10005", "10006", "10007", "10008",
            "10009"},
        FakeRow{"2000", "2001", "20002", "20003", "20004", "20005", "20006", "20007", "20008",
            "20009"},
    });
    std::vector<PetLevelStatsRow> rows = CreatureDataRepository::QueryPetLevelStats();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].creatureEntry, 1000u);
    CHECK_EQ(rows[0].level, 1001u);
    CHECK_EQ(rows[0].hp, uint16(10002));
    CHECK_EQ(rows[0].mana, uint16(10003));
    CHECK_EQ(rows[0].stats[0], uint16(10004));
    CHECK_EQ(rows[0].stats[1], uint16(10005));
    CHECK_EQ(rows[0].stats[2], uint16(10006));
    CHECK_EQ(rows[0].stats[3], uint16(10007));
    CHECK_EQ(rows[0].stats[4], uint16(10008));
    CHECK_EQ(rows[0].armor, uint16(10009));
    CHECK_EQ(rows[1].creatureEntry, 2000u);
}

// Each column of a `creature_linking_template` row lands in its member, and the rows come back in
// order.
TEST(CreatureDataRepository_LinkingTemplates)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "10003", "10004"},
        FakeRow{"2000", "2001", "2002", "20003", "20004"},
    });
    std::vector<CreatureLinkingTemplateRow> rows = CreatureDataRepository::QueryLinkingTemplates();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].map, 1001u);
    CHECK_EQ(rows[0].masterEntry, 1002u);
    CHECK_EQ(rows[0].flag, uint16(10003));
    CHECK_EQ(rows[0].searchRange, uint16(10004));
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `creature_linking` row lands in its member, and the rows come back in order.
TEST(CreatureDataRepository_Linkings)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "10002"},
        FakeRow{"2000", "2001", "20002"},
    });
    std::vector<CreatureLinkingRow> rows = CreatureDataRepository::QueryLinkings();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].masterGuid, 1001u);
    CHECK_EQ(rows[0].flag, uint16(10002));
    CHECK_EQ(rows[1].guid, 2000u);
}
