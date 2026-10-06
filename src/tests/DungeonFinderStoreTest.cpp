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

/// The dungeon finder's requirements, rewards and item rewards, set and looked up with no
/// database: a miss and a hit for each map (two dungeons on one map differing only in difficulty
/// stay distinct), a second set for a key overwriting the first, the whole-map accessors, and
/// each clear forgetting its own map only.

#include "TestHarness.h"
#include "data/DungeonFinderStore.h"

static char const* const kQuestText = "You must complete the quest first.";

TEST(DungeonFinderStore_RequirementsMissAnswersNull)
{
    DungeonFinderStore store;
    CHECK(store.FindRequirements(33, 1) == NULL);

    store.SetRequirements(MAKE_PAIR32(33, 1), DungeonFinderRequirements(180, 0, 0, 0, 0, 0, NULL));
    CHECK(store.FindRequirements(33, 1) != NULL);
    CHECK(store.FindRequirements(33, 0) == NULL);
    CHECK(store.FindRequirements(33, 2) == NULL);
    CHECK(store.FindRequirements(34, 1) == NULL);
    CHECK(store.FindRequirements(1, 33) == NULL);
}

TEST(DungeonFinderStore_RequirementsHitThroughThePairKey)
{
    DungeonFinderStore store;
    store.SetRequirements(MAKE_PAIR32(36, 0), DungeonFinderRequirements(0, 0, 0, 0, 0, 0, NULL));
    store.SetRequirements(MAKE_PAIR32(36, 1), DungeonFinderRequirements(329, 7, 8, 11, 12, 4840, kQuestText));

    DungeonFinderRequirements const* normal = store.FindRequirements(36, 0);
    DungeonFinderRequirements const* heroic = store.FindRequirements(36, 1);
    REQUIRE(normal != NULL);
    REQUIRE(heroic != NULL);
    CHECK(normal != heroic);

    CHECK_EQ(normal->minItemLevel, 0u);
    CHECK_EQ(normal->achievement, 0u);
    CHECK(normal->questIncompleteText == NULL);

    CHECK_EQ(heroic->minItemLevel, 329u);
    CHECK_EQ(heroic->item, 7u);
    CHECK_EQ(heroic->item2, 8u);
    CHECK_EQ(heroic->allianceQuestId, 11u);
    CHECK_EQ(heroic->hordeQuestId, 12u);
    CHECK_EQ(heroic->achievement, 4840u);
    CHECK(heroic->questIncompleteText == kQuestText);
}

TEST(DungeonFinderStore_RewardsMissAndHit)
{
    DungeonFinderStore store;
    CHECK(store.FindRewards(15) == NULL);

    store.SetRewards(15, DungeonFinderRewards(4500, -200));
    store.SetRewards(85, DungeonFinderRewards(0, 0));
    CHECK(store.FindRewards(14) == NULL);
    CHECK(store.FindRewards(16) == NULL);

    DungeonFinderRewards const* rewards = store.FindRewards(15);
    REQUIRE(rewards != NULL);
    CHECK_EQ(rewards->baseXPReward, 4500u);
    CHECK_EQ(rewards->baseMonetaryReward, -200);
}

TEST(DungeonFinderStore_ItemsMissAndHit)
{
    DungeonFinderStore store;
    CHECK(store.Items().empty());

    store.SetItems(3, DungeonFinderItems(15, 24, 51999, 1, 1));
    store.SetItems(4, DungeonFinderItems(80, 85, 52005, 2, 2));
    CHECK(store.Items().find(2) == store.Items().end());
    CHECK(store.Items().find(5) == store.Items().end());

    DungeonFinderItemsMap::const_iterator itr = store.Items().find(3);
    REQUIRE(itr != store.Items().end());
    CHECK_EQ(itr->second.minLevel, 15u);
    CHECK_EQ(itr->second.maxLevel, 24u);
    CHECK_EQ(itr->second.itemReward, 51999u);
    CHECK_EQ(itr->second.itemAmount, 1u);
    CHECK_EQ(itr->second.dungeonType, 1u);
}

TEST(DungeonFinderStore_SecondSetOverwrites)
{
    DungeonFinderStore store;
    store.SetRequirements(MAKE_PAIR32(43, 0), DungeonFinderRequirements(10, 0, 0, 0, 0, 0, NULL));
    store.SetRequirements(MAKE_PAIR32(43, 0), DungeonFinderRequirements(20, 0, 0, 0, 0, 0, NULL));
    store.SetRewards(60, DungeonFinderRewards(100, 1));
    store.SetRewards(60, DungeonFinderRewards(200, 2));
    store.SetItems(9, DungeonFinderItems(1, 2, 3, 4, 5));
    store.SetItems(9, DungeonFinderItems(6, 7, 8, 9, 10));

    DungeonFinderRequirements const* requirements = store.FindRequirements(43, 0);
    DungeonFinderRewards const* rewards = store.FindRewards(60);
    REQUIRE(requirements != NULL);
    REQUIRE(rewards != NULL);
    CHECK_EQ(requirements->minItemLevel, 20u);
    CHECK_EQ(rewards->baseXPReward, 200u);
    CHECK_EQ(rewards->baseMonetaryReward, 2);
    CHECK_EQ(store.Requirements().size(), size_t(1));
    CHECK_EQ(store.Rewards().size(), size_t(1));
    REQUIRE(store.Items().size() == 1);
    CHECK_EQ(store.Items().find(9)->second.itemReward, 8u);
}

TEST(DungeonFinderStore_WholeMapAccessorsHoldTheStoredEntries)
{
    DungeonFinderStore store;
    store.SetRequirements(MAKE_PAIR32(33, 0), DungeonFinderRequirements(1, 0, 0, 0, 0, 0, NULL));
    store.SetRequirements(MAKE_PAIR32(33, 1), DungeonFinderRequirements(2, 0, 0, 0, 0, 0, NULL));
    store.SetRewards(10, DungeonFinderRewards(30, 0));
    store.SetItems(1, DungeonFinderItems(15, 24, 51999, 1, 1));
    store.SetItems(2, DungeonFinderItems(80, 85, 52005, 2, 2));

    REQUIRE(store.Requirements().size() == 2);
    CHECK_EQ(store.Requirements().find(MAKE_PAIR32(33, 0))->second.minItemLevel, 1u);
    CHECK_EQ(store.Requirements().find(MAKE_PAIR32(33, 1))->second.minItemLevel, 2u);
    REQUIRE(store.Rewards().size() == 1);
    CHECK_EQ(store.Rewards().find(10)->second.baseXPReward, 30u);
    REQUIRE(store.Items().size() == 2);
    CHECK_EQ(store.Items().find(1)->second.itemReward, 51999u);
    CHECK_EQ(store.Items().find(2)->second.itemReward, 52005u);
}

static void FillEveryMap(DungeonFinderStore& store)
{
    store.SetRequirements(MAKE_PAIR32(33, 0), DungeonFinderRequirements(1, 0, 0, 0, 0, 0, NULL));
    store.SetRewards(10, DungeonFinderRewards(30, 0));
    store.SetItems(1, DungeonFinderItems(15, 24, 51999, 1, 1));
}

TEST(DungeonFinderStore_EachClearForgetsItsOwnMap)
{
    DungeonFinderStore store;
    FillEveryMap(store);
    store.ClearRequirements();
    CHECK(store.FindRequirements(33, 0) == NULL);
    CHECK(store.FindRewards(10) != NULL);
    CHECK_EQ(store.Items().size(), size_t(1));

    FillEveryMap(store);
    store.ClearRewards();
    CHECK(store.FindRewards(10) == NULL);
    CHECK(store.FindRequirements(33, 0) != NULL);
    CHECK_EQ(store.Items().size(), size_t(1));

    FillEveryMap(store);
    store.ClearItems();
    CHECK(store.Items().empty());
    CHECK(store.FindRequirements(33, 0) != NULL);
    CHECK(store.FindRewards(10) != NULL);
}
