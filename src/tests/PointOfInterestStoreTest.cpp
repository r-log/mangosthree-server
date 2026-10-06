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

/// The points of interest, set and looked up with no database: an id with no point, an id with
/// one (every field, the icon name included), a second point for the same id, two ids side by
/// side, and a cleared store.

#include "TestHarness.h"
#include "data/PointOfInterestStore.h"

#include <string>

static PointOfInterest MakePoi(uint32 entry, float x, float y, uint32 icon, uint32 flags, uint32 data, std::string const& iconName)
{
    PointOfInterest poi;
    poi.entry = entry;
    poi.x = x;
    poi.y = y;
    poi.icon = icon;
    poi.flags = flags;
    poi.data = data;
    poi.icon_name = iconName;
    return poi;
}

TEST(PointOfInterestStore_IdWithNoPointAnswersNull)
{
    PointOfInterestStore store;
    CHECK(store.Find(7) == NULL);

    store.Set(7, MakePoi(7, 1.0f, 2.0f, 6, 99, 0, "Bank"));
    CHECK(store.Find(7) != NULL);
    CHECK(store.Find(6) == NULL);
    CHECK(store.Find(8) == NULL);
}

TEST(PointOfInterestStore_IdWithPointAnswersItsFields)
{
    PointOfInterestStore store;
    store.Set(42, MakePoi(42, -8851.5f, 649.25f, 7, 6, 1, "Stormwind Auction House"));

    PointOfInterest const* poi = store.Find(42);
    REQUIRE(poi != NULL);
    CHECK_EQ(poi->entry, 42u);
    CHECK_EQ(poi->x, -8851.5f);
    CHECK_EQ(poi->y, 649.25f);
    CHECK_EQ(poi->icon, 7u);
    CHECK_EQ(poi->flags, 6u);
    CHECK_EQ(poi->data, 1u);
    CHECK_STR(poi->icon_name, "Stormwind Auction House");
}

TEST(PointOfInterestStore_SecondSetOverwrites)
{
    PointOfInterestStore store;
    store.Set(5, MakePoi(5, 10.0f, 20.0f, 6, 99, 0, "Old"));
    store.Set(5, MakePoi(5, 30.0f, 40.0f, 7, 6, 2, "New"));

    PointOfInterest const* poi = store.Find(5);
    REQUIRE(poi != NULL);
    CHECK_EQ(poi->x, 30.0f);
    CHECK_EQ(poi->y, 40.0f);
    CHECK_EQ(poi->icon, 7u);
    CHECK_EQ(poi->data, 2u);
    CHECK_STR(poi->icon_name, "New");
}

TEST(PointOfInterestStore_IdsStayIndependent)
{
    PointOfInterestStore store;
    store.Set(1, MakePoi(1, 1.0f, 1.0f, 6, 99, 0, "First"));
    store.Set(2, MakePoi(2, 2.0f, 2.0f, 7, 6, 0, "Second"));
    store.Set(1, MakePoi(1, 3.0f, 3.0f, 8, 6, 0, "Third"));

    PointOfInterest const* first = store.Find(1);
    PointOfInterest const* second = store.Find(2);
    REQUIRE(first != NULL);
    REQUIRE(second != NULL);
    CHECK_STR(first->icon_name, "Third");
    CHECK_EQ(second->x, 2.0f);
    CHECK_EQ(second->icon, 7u);
    CHECK_STR(second->icon_name, "Second");
}

TEST(PointOfInterestStore_ClearForgetsEveryId)
{
    PointOfInterestStore store;
    store.Set(1, MakePoi(1, 1.0f, 1.0f, 6, 99, 0, "First"));
    store.Set(2, MakePoi(2, 2.0f, 2.0f, 7, 6, 0, "Second"));
    store.Clear();

    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) == NULL);

    store.Set(2, MakePoi(2, 5.0f, 5.0f, 7, 6, 0, "Again"));
    CHECK(store.Find(2) != NULL);
    CHECK(store.Find(1) == NULL);
}
