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

/// The named teleport locations, set and looked up with no database: an id with no location, an id
/// with one, a second location for the same id, an erased id, and a cleared store.

#include "TestHarness.h"
#include "data/GameTeleStore.h"

#include <string>

namespace
{
    GameTele MakeTele(uint32 mapId, float x, std::string const& name)
    {
        GameTele tele = GameTele();
        tele.mapId = mapId;
        tele.position_x = x;
        tele.name = name;
        return tele;
    }
}

TEST(GameTeleStore_AnIdAnswersItsLocation)
{
    GameTeleStore store;
    CHECK(store.Find(3) == NULL);

    store.Set(3, MakeTele(0, -8913.25f, "Stormwind"));
    GameTele const* tele = store.Find(3);
    REQUIRE(tele != NULL);
    CHECK_EQ(tele->mapId, uint32(0));
    CHECK_EQ(tele->position_x, -8913.25f);
    CHECK_STR(tele->name, "Stormwind");
    CHECK(store.Find(4) == NULL);
}

TEST(GameTeleStore_ASecondLocationForAnIdOverwrites)
{
    GameTeleStore store;
    store.Set(3, MakeTele(0, 1.0f, "Old"));
    store.Set(3, MakeTele(1, 2.0f, "New"));

    GameTele const* tele = store.Find(3);
    REQUIRE(tele != NULL);
    CHECK_EQ(tele->mapId, uint32(1));
    CHECK_STR(tele->name, "New");
    CHECK_EQ(store.All().size(), size_t(1));
}

TEST(GameTeleStore_ErasingAndClearingForgetIds)
{
    GameTeleStore store;
    store.Set(1, MakeTele(0, 1.0f, "First"));
    store.Set(2, MakeTele(1, 2.0f, "Second"));

    store.Erase(1);
    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) != NULL);
    store.Erase(9);
    CHECK_EQ(store.All().size(), size_t(1));

    store.Clear();
    CHECK(store.All().empty());
    CHECK(store.Find(2) == NULL);
}
