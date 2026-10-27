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

/// The instance templates, set and looked up with no database: a map with no template, a map with
/// one (every field), a second template for the same map, the highest map, an edited parent, an
/// erased map, and a cleared store.

#include "TestHarness.h"
#include "data/InstanceTemplateStore.h"

static InstanceTemplate MakeInstance(uint32 map, uint32 parent, uint32 levelMin, uint32 levelMax)
{
    InstanceTemplate instance;
    instance.map = map;
    instance.parent = parent;
    instance.levelMin = levelMin;
    instance.levelMax = levelMax;
    return instance;
}

TEST(InstanceTemplateStore_MapWithNoTemplateAnswersNull)
{
    InstanceTemplateStore store;
    CHECK(store.Find(33) == NULL);
    CHECK(store.FindForEdit(33) == NULL);

    store.Set(MakeInstance(33, 0, 14, 21));
    CHECK(store.Find(33) != NULL);
    CHECK(store.Find(32) == NULL);
    CHECK(store.Find(34) == NULL);
}

TEST(InstanceTemplateStore_MapWithTemplateAnswersItsFields)
{
    InstanceTemplateStore store;
    store.Set(MakeInstance(560, 530, 64, 70));

    InstanceTemplate const* instance = store.Find(560);
    REQUIRE(instance != NULL);
    CHECK_EQ(instance->map, 560u);
    CHECK_EQ(instance->parent, 530u);
    CHECK_EQ(instance->levelMin, 64u);
    CHECK_EQ(instance->levelMax, 70u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(InstanceTemplateStore_SecondSetOverwrites)
{
    InstanceTemplateStore store;
    store.Set(MakeInstance(36, 0, 10, 20));
    store.Set(MakeInstance(36, 0, 15, 25));

    InstanceTemplate const* instance = store.Find(36);
    REQUIRE(instance != NULL);
    CHECK_EQ(instance->levelMin, 15u);
    CHECK_EQ(instance->levelMax, 25u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(InstanceTemplateStore_MaxEntryIsOnePastTheHighestMap)
{
    InstanceTemplateStore store;
    CHECK_EQ(store.GetMaxEntry(), 0u);

    store.Set(MakeInstance(725, 0, 82, 85));
    store.Set(MakeInstance(33, 0, 14, 21));
    CHECK_EQ(store.GetMaxEntry(), 726u);
}

TEST(InstanceTemplateStore_EditAndEraseChangeTheStore)
{
    InstanceTemplateStore store;
    store.Set(MakeInstance(540, 530, 68, 70));
    store.Set(MakeInstance(541, 530, 68, 70));

    InstanceTemplate* instance = store.FindForEdit(540);
    REQUIRE(instance != NULL);
    instance->parent = 0;
    CHECK_EQ(store.Find(540)->parent, 0u);
    CHECK_EQ(store.Find(541)->parent, 530u);

    store.Erase(541);
    CHECK(store.Find(541) == NULL);
    CHECK(store.Find(540) != NULL);
    CHECK_EQ(store.GetCount(), 1u);
    CHECK_EQ(store.GetMaxEntry(), 541u);
}

TEST(InstanceTemplateStore_ClearForgetsEveryMap)
{
    InstanceTemplateStore store;
    store.Set(MakeInstance(33, 0, 14, 21));
    store.Set(MakeInstance(36, 0, 15, 25));
    store.Clear();

    CHECK(store.Find(33) == NULL);
    CHECK(store.Find(36) == NULL);
    CHECK_EQ(store.GetCount(), 0u);
    CHECK_EQ(store.GetMaxEntry(), 0u);
}
