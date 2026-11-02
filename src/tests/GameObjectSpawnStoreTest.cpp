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

/// The game object spawns, filled and looked up with no database: a guid with no spawn, a spawn
/// filled in place and found with its guid, a second fill of the same guid, and an erased spawn.

#include "TestHarness.h"
#include "data/GameObjectSpawnStore.h"

TEST(GameObjectSpawnStore_AGuidWithNoSpawnAnswersNull)
{
    GameObjectSpawnStore store;
    CHECK(store.Find(2000) == NULL);
    CHECK(store.All().empty());
}

TEST(GameObjectSpawnStore_ASpawnIsFilledInPlaceAndFoundWithItsGuid)
{
    GameObjectSpawnStore store;
    GameObjectData& data = store.FindOrAdd(2000);
    data.id = 1731;
    data.mapid = 1;

    GameObjectDataPair const* found = store.Find(2000);
    REQUIRE(found != NULL);
    CHECK_EQ(found->first, uint32(2000));
    CHECK_EQ(found->second.id, uint32(1731));
    CHECK(store.Find(2001) == NULL);

    store.FindOrAdd(2000).mapid = 0;
    CHECK_EQ(store.Find(2000)->second.mapid, uint32(0));
    CHECK_EQ(store.Find(2000)->second.id, uint32(1731));
    CHECK_EQ(store.All().size(), size_t(1));
}

TEST(GameObjectSpawnStore_AnErasedSpawnIsGone)
{
    GameObjectSpawnStore store;
    store.FindOrAdd(2000).id = 1731;
    store.FindOrAdd(2001).id = 1732;

    store.Erase(2000);
    CHECK(store.Find(2000) == NULL);
    REQUIRE(store.Find(2001) != NULL);
    CHECK_EQ(store.All().size(), size_t(1));
}
