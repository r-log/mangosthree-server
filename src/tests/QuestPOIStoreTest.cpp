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

/// The quest points of interest, added and looked up with no database: a quest with no POI, a
/// quest with one (its fields), two POIs for one quest in the order added, a point pushed through
/// Edit, Edit on a quest with no POI, two quests side by side, and a cleared store.

#include "TestHarness.h"
#include "data/QuestPOIStore.h"

TEST(QuestPOIStore_QuestWithNoPoiAnswersNull)
{
    QuestPOIStore store;
    CHECK(store.Find(100) == NULL);

    store.Add(100, QuestPOI(1, 0, 0, 12, 0, 0, 0));
    CHECK(store.Find(100) != NULL);
    CHECK(store.Find(99) == NULL);
    CHECK(store.Find(101) == NULL);
}

TEST(QuestPOIStore_AddThenFindAnswersThePoi)
{
    QuestPOIStore store;
    store.Add(26391, QuestPOI(7, -1, 654, 611, 2, 3, 4));

    QuestPOIVector const* pois = store.Find(26391);
    REQUIRE(pois != NULL);
    REQUIRE(pois->size() == 1);
    QuestPOI const& poi = (*pois)[0];
    CHECK_EQ(poi.PoiId, 7u);
    CHECK_EQ(poi.ObjectiveIndex, -1);
    CHECK_EQ(poi.MapId, 654u);
    CHECK_EQ(poi.MapAreaId, 611u);
    CHECK_EQ(poi.FloorId, 2u);
    CHECK_EQ(poi.Unk3, 3u);
    CHECK_EQ(poi.Unk4, 4u);
    CHECK(poi.points.empty());
}

TEST(QuestPOIStore_TwoAddsKeepInsertionOrder)
{
    QuestPOIStore store;
    store.Add(200, QuestPOI(5, 1, 0, 12, 0, 0, 0));
    store.Add(200, QuestPOI(2, 0, 0, 12, 0, 0, 0));
    store.Add(300, QuestPOI(9, 0, 1, 14, 0, 0, 0));

    QuestPOIVector const* pois = store.Find(200);
    REQUIRE(pois != NULL);
    REQUIRE(pois->size() == 2);
    CHECK_EQ((*pois)[0].PoiId, 5u);
    CHECK_EQ((*pois)[1].PoiId, 2u);

    QuestPOIVector const* other = store.Find(300);
    REQUIRE(other != NULL);
    REQUIRE(other->size() == 1);
    CHECK_EQ((*other)[0].PoiId, 9u);
}

TEST(QuestPOIStore_EditOnKnownQuestReturnsTheAddedPois)
{
    QuestPOIStore store;
    store.Add(400, QuestPOI(1, 0, 0, 12, 0, 0, 0));
    store.Add(400, QuestPOI(2, 1, 0, 12, 0, 0, 0));

    QuestPOIVector& vect = store.Edit(400);
    REQUIRE(vect.size() == 2);
    vect[1].points.push_back(QuestPOIPoint(-9000, 250));

    QuestPOIVector const* pois = store.Find(400);
    REQUIRE(pois != NULL);
    REQUIRE(pois->size() == 2);
    CHECK((*pois)[0].points.empty());
    REQUIRE((*pois)[1].points.size() == 1);
    CHECK_EQ((*pois)[1].points[0].x, -9000);
    CHECK_EQ((*pois)[1].points[0].y, 250);
}

TEST(QuestPOIStore_EditOnUnknownQuestCreatesAnEmptyEntry)
{
    QuestPOIStore store;
    CHECK(store.Find(500) == NULL);

    QuestPOIVector& vect = store.Edit(500);
    CHECK(vect.empty());

    QuestPOIVector const* pois = store.Find(500);
    REQUIRE(pois != NULL);
    CHECK(pois->empty());
    CHECK(store.Find(501) == NULL);
}

TEST(QuestPOIStore_ClearForgetsEveryQuest)
{
    QuestPOIStore store;
    store.Add(600, QuestPOI(1, 0, 0, 12, 0, 0, 0));
    store.Add(700, QuestPOI(2, 0, 0, 12, 0, 0, 0));
    store.Clear();

    CHECK(store.Find(600) == NULL);
    CHECK(store.Find(700) == NULL);

    store.Add(700, QuestPOI(3, 0, 0, 12, 0, 0, 0));
    QuestPOIVector const* pois = store.Find(700);
    REQUIRE(pois != NULL);
    REQUIRE(pois->size() == 1);
    CHECK_EQ((*pois)[0].PoiId, 3u);
    CHECK(store.Find(600) == NULL);
}
