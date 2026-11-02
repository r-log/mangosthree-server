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

/// The area triggers, set and looked up with no database: a teleport by trigger id, a second teleport
/// for the same id, the quest a trigger serves, the taverns, and each part cleared on its own.

#include "TestHarness.h"
#include "data/AreaTriggerStore.h"

namespace
{
    AreaTrigger MakeTeleport(uint32 mapId, float x, uint8 requiredLevel)
    {
        AreaTrigger at = AreaTrigger();
        at.target_mapId = mapId;
        at.target_X = x;
        at.requiredLevel = requiredLevel;
        return at;
    }
}

TEST(AreaTriggerStore_ATriggerAnswersItsTeleport)
{
    AreaTriggerStore store;
    CHECK(store.FindTeleport(45) == NULL);

    store.SetTeleport(45, MakeTeleport(189, 1688.5f, 0));
    AreaTrigger const* at = store.FindTeleport(45);
    REQUIRE(at != NULL);
    CHECK_EQ(at->target_mapId, uint32(189));
    CHECK_EQ(at->target_X, 1688.5f);
    CHECK(at->IsMinimal());
    CHECK(store.FindTeleport(46) == NULL);
    CHECK_EQ(store.Teleports().size(), size_t(1));
}

TEST(AreaTriggerStore_ASecondTeleportForATriggerOverwrites)
{
    AreaTriggerStore store;
    store.SetTeleport(45, MakeTeleport(189, 1.0f, 0));
    store.SetTeleport(45, MakeTeleport(189, 2.0f, 30));

    AreaTrigger const* at = store.FindTeleport(45);
    REQUIRE(at != NULL);
    CHECK_EQ(at->target_X, 2.0f);
    CHECK(!at->IsMinimal());
    CHECK_EQ(store.Teleports().size(), size_t(1));
}

TEST(AreaTriggerStore_ATriggerAnswersItsQuestAndTavern)
{
    AreaTriggerStore store;
    CHECK_EQ(store.QuestFor(171), uint32(0));
    CHECK(!store.IsTavern(171));

    store.SetQuestTrigger(171, 62);
    store.AddTavern(562);
    CHECK_EQ(store.QuestFor(171), uint32(62));
    CHECK_EQ(store.QuestFor(562), uint32(0));
    CHECK(store.IsTavern(562));
    CHECK(!store.IsTavern(171));
}

TEST(AreaTriggerStore_EachPartClearsOnItsOwn)
{
    AreaTriggerStore store;
    store.SetTeleport(45, MakeTeleport(189, 1.0f, 0));
    store.SetQuestTrigger(171, 62);
    store.AddTavern(562);

    store.ClearQuestTriggers();
    CHECK_EQ(store.QuestFor(171), uint32(0));
    CHECK(store.FindTeleport(45) != NULL);
    CHECK(store.IsTavern(562));

    store.ClearTaverns();
    CHECK(!store.IsTavern(562));
    CHECK(store.FindTeleport(45) != NULL);

    store.ClearTeleports();
    CHECK(store.FindTeleport(45) == NULL);
    CHECK(store.Teleports().empty());
}
