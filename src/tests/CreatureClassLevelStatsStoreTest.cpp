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

/// The creature base stats, set and looked up with no database: each expansion's slot answers its own
/// stats, a slot never set answers zeros, a level, class index or expansion past the last answers
/// NULL, and a cleared store answers zeros again.

#include "TestHarness.h"
#include "data/CreatureClassLevelStatsStore.h"

namespace
{
    CreatureClassLvlStats MakeStats(uint32 health, float damage)
    {
        CreatureClassLvlStats stats;
        stats.BaseHealth = health;
        stats.BaseMana = 0;
        stats.BaseDamage = damage;
        stats.BaseMeleeAttackPower = 0.0f;
        stats.BaseRangedAttackPower = 0.0f;
        stats.BaseArmor = 0;
        return stats;
    }
}

TEST(CreatureClassLevelStatsStore_EachExpansionAnswersItsOwnStats)
{
    CreatureClassLevelStatsStore store;
    store.Set(70, 0, 0, MakeStats(4050, 10.0f));
    store.Set(70, 0, 1, MakeStats(6986, 20.0f));
    store.Set(70, 0, 2, MakeStats(8982, 30.0f));
    store.Set(70, 1, 0, MakeStats(3500, 9.0f));

    REQUIRE(store.Find(70, 0, 0) != NULL);
    CHECK_EQ(store.Find(70, 0, 0)->BaseHealth, uint32(4050));
    CHECK_EQ(store.Find(70, 0, 1)->BaseHealth, uint32(6986));
    CHECK_EQ(store.Find(70, 0, 2)->BaseHealth, uint32(8982));
    CHECK_EQ(store.Find(70, 1, 0)->BaseHealth, uint32(3500));
    CHECK_EQ(store.Find(70, 0, 3)->BaseHealth, uint32(0));
}

TEST(CreatureClassLevelStatsStore_ASlotNeverSetAnswersZeros)
{
    CreatureClassLevelStatsStore store;
    CreatureClassLvlStats const* stats = store.Find(1, 0, 0);
    REQUIRE(stats != NULL);
    CHECK_EQ(stats->BaseHealth, uint32(0));
    CHECK_EQ(stats->BaseDamage, 0.0f);
}

TEST(CreatureClassLevelStatsStore_PastTheLastAnswersNull)
{
    CreatureClassLevelStatsStore store;
    CHECK(store.Find(DEFAULT_MAX_CREATURE_LEVEL, MAX_CREATURE_CLASS - 1, MAX_EXPANSION) != NULL);
    CHECK(store.Find(DEFAULT_MAX_CREATURE_LEVEL + 1, 0, 0) == NULL);
    CHECK(store.Find(1, MAX_CREATURE_CLASS, 0) == NULL);
    CHECK(store.Find(1, 0, MAX_EXPANSION + 1) == NULL);

    store.Set(DEFAULT_MAX_CREATURE_LEVEL + 1, 0, 0, MakeStats(1, 1.0f));
    store.Set(1, 0, MAX_EXPANSION + 1, MakeStats(1, 1.0f));
    CHECK_EQ(store.Find(1, 0, MAX_EXPANSION)->BaseHealth, uint32(0));
    CHECK_EQ(store.Find(2, 0, 0)->BaseHealth, uint32(0));
}

TEST(CreatureClassLevelStatsStore_ClearingForgetsEveryStat)
{
    CreatureClassLevelStatsStore store;
    store.Set(70, 0, 0, MakeStats(4050, 10.0f));
    store.Clear();
    CHECK_EQ(store.Find(70, 0, 0)->BaseHealth, uint32(0));
}
