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

/// The LFG dungeon entrances, set and looked up with no database: a dungeon with no entrance, a
/// dungeon with one (the four floats), a second entrance for the same dungeon, two dungeons side by
/// side, and a cleared store.

#include "TestHarness.h"
#include "data/LfgDungeonEntranceStore.h"

static LfgDungeonEntrance MakeEntrance(float x, float y, float z, float o)
{
    LfgDungeonEntrance entrance;
    entrance.x = x;
    entrance.y = y;
    entrance.z = z;
    entrance.o = o;
    return entrance;
}

TEST(LfgDungeonEntranceStore_DungeonWithNoEntranceAnswersNull)
{
    LfgDungeonEntranceStore store;
    CHECK(store.Find(261) == NULL);

    store.Set(261, MakeEntrance(1.0f, 2.0f, 3.0f, 0.5f));
    CHECK(store.Find(261) != NULL);
    CHECK(store.Find(260) == NULL);
    CHECK(store.Find(262) == NULL);
}

TEST(LfgDungeonEntranceStore_DungeonWithEntranceAnswersItsFields)
{
    LfgDungeonEntranceStore store;
    store.Set(285, MakeEntrance(-8762.25f, 848.5f, 88.0f, 0.785f));

    LfgDungeonEntrance const* entrance = store.Find(285);
    REQUIRE(entrance != NULL);
    CHECK_EQ(entrance->x, -8762.25f);
    CHECK_EQ(entrance->y, 848.5f);
    CHECK_EQ(entrance->z, 88.0f);
    CHECK_EQ(entrance->o, 0.785f);
}

TEST(LfgDungeonEntranceStore_SecondSetOverwrites)
{
    LfgDungeonEntranceStore store;
    store.Set(18, MakeEntrance(10.0f, 20.0f, 30.0f, 1.0f));
    store.Set(18, MakeEntrance(40.0f, 50.0f, 60.0f, 2.0f));

    LfgDungeonEntrance const* entrance = store.Find(18);
    REQUIRE(entrance != NULL);
    CHECK_EQ(entrance->x, 40.0f);
    CHECK_EQ(entrance->y, 50.0f);
    CHECK_EQ(entrance->z, 60.0f);
    CHECK_EQ(entrance->o, 2.0f);
}

TEST(LfgDungeonEntranceStore_DungeonsStayIndependent)
{
    LfgDungeonEntranceStore store;
    store.Set(1, MakeEntrance(1.0f, 1.0f, 1.0f, 1.0f));
    store.Set(2, MakeEntrance(2.0f, 2.0f, 2.0f, 2.0f));
    store.Set(1, MakeEntrance(3.0f, 3.0f, 3.0f, 3.0f));

    LfgDungeonEntrance const* first = store.Find(1);
    LfgDungeonEntrance const* second = store.Find(2);
    REQUIRE(first != NULL);
    REQUIRE(second != NULL);
    CHECK_EQ(first->x, 3.0f);
    CHECK_EQ(first->o, 3.0f);
    CHECK_EQ(second->x, 2.0f);
    CHECK_EQ(second->o, 2.0f);
}

TEST(LfgDungeonEntranceStore_ClearForgetsEveryDungeon)
{
    LfgDungeonEntranceStore store;
    store.Set(1, MakeEntrance(1.0f, 1.0f, 1.0f, 1.0f));
    store.Set(2, MakeEntrance(2.0f, 2.0f, 2.0f, 2.0f));
    store.Clear();

    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) == NULL);

    store.Set(2, MakeEntrance(5.0f, 5.0f, 5.0f, 5.0f));
    CHECK(store.Find(2) != NULL);
    CHECK(store.Find(1) == NULL);
}
