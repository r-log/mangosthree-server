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

/// The creature spawns, filled and looked up with no database: a guid with no spawn, a spawn filled
/// in place and found with its guid, a second fill of the same guid, and an erased spawn.

#include "TestHarness.h"
#include "data/CreatureSpawnStore.h"

TEST(CreatureSpawnStore_AGuidWithNoSpawnAnswersNull)
{
    CreatureSpawnStore store;
    CHECK(store.Find(1000) == NULL);
    CHECK(store.All().empty());
}

TEST(CreatureSpawnStore_ASpawnIsFilledInPlaceAndFoundWithItsGuid)
{
    CreatureSpawnStore store;
    CreatureData& data = store.FindOrAdd(1000);
    data.id = 68;
    data.mapid = 0;

    CreatureDataPair const* found = store.Find(1000);
    REQUIRE(found != NULL);
    CHECK_EQ(found->first, uint32(1000));
    CHECK_EQ(found->second.id, uint32(68));
    CHECK(store.Find(1001) == NULL);

    CreatureData& again = store.FindOrAdd(1000);
    CHECK_EQ(again.id, uint32(68));
    again.id = 69;
    CHECK_EQ(store.Find(1000)->second.id, uint32(69));
    CHECK_EQ(store.All().size(), size_t(1));
}

TEST(CreatureSpawnStore_AnErasedSpawnIsGone)
{
    CreatureSpawnStore store;
    store.FindOrAdd(1000).id = 68;
    store.FindOrAdd(1001).id = 69;

    store.Erase(1000);
    CHECK(store.Find(1000) == NULL);
    REQUIRE(store.Find(1001) != NULL);
    CHECK_EQ(store.Find(1001)->second.id, uint32(69));
    store.Erase(5);
    CHECK_EQ(store.All().size(), size_t(1));
}
