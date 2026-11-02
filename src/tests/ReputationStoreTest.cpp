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

/// The reputation tables, set and looked up with no database: a key with no record, a key with one,
/// a second record for the same key, and the reward rates and spillovers cleared on their own.

#include "TestHarness.h"
#include "data/ReputationStore.h"

namespace
{
    RepRewardRate MakeRate(float questRate)
    {
        RepRewardRate rate = RepRewardRate();
        rate.quest_rate = questRate;
        return rate;
    }
}

TEST(ReputationStore_AKeyWithNoRecordAnswersNull)
{
    ReputationStore store;
    CHECK(store.RewardRate(72) == NULL);
    CHECK(store.OnKill(1000) == NULL);
    CHECK(store.Spillover(72) == NULL);
}

TEST(ReputationStore_EachPartAnswersItsOwnRecords)
{
    ReputationStore store;
    store.SetRewardRate(72, MakeRate(2.0f));
    ReputationOnKillEntry onKill = ReputationOnKillEntry();
    onKill.repfaction1 = 72;
    onKill.repvalue1 = 25;
    store.SetOnKill(1000, onKill);
    RepSpilloverTemplate spillover = RepSpilloverTemplate();
    spillover.faction[0] = 47;
    store.SetSpillover(72, spillover);

    REQUIRE(store.RewardRate(72) != NULL);
    CHECK_EQ(store.RewardRate(72)->quest_rate, 2.0f);
    REQUIRE(store.OnKill(1000) != NULL);
    CHECK_EQ(store.OnKill(1000)->repvalue1, int32(25));
    CHECK(store.OnKill(72) == NULL);
    REQUIRE(store.Spillover(72) != NULL);
    CHECK_EQ(store.Spillover(72)->faction[0], uint32(47));

    store.SetRewardRate(72, MakeRate(3.0f));
    CHECK_EQ(store.RewardRate(72)->quest_rate, 3.0f);
}

TEST(ReputationStore_TheRatesAndSpilloversClearOnTheirOwn)
{
    ReputationStore store;
    store.SetRewardRate(72, MakeRate(2.0f));
    store.SetOnKill(1000, ReputationOnKillEntry());
    store.SetSpillover(72, RepSpilloverTemplate());

    store.ClearRewardRates();
    CHECK(store.RewardRate(72) == NULL);
    CHECK(store.Spillover(72) != NULL);

    store.ClearSpillovers();
    CHECK(store.Spillover(72) == NULL);
    CHECK(store.OnKill(1000) != NULL);
}
