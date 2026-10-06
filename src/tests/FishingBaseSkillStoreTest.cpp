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

/// The fishing base skill levels, set and looked up with no database: an area with no value, an
/// area with one (a negative value included), a second value for the same area, and a cleared
/// store.

#include "TestHarness.h"
#include "data/FishingBaseSkillStore.h"

TEST(FishingBaseSkillStore_AreaWithNoValueAnswersZero)
{
    FishingBaseSkillStore store;
    CHECK_EQ(store.Get(12), 0);

    store.Set(12, 25);
    CHECK_EQ(store.Get(11), 0);
    CHECK_EQ(store.Get(13), 0);
}

TEST(FishingBaseSkillStore_AreaWithValueAnswersIt)
{
    FishingBaseSkillStore store;
    store.Set(1519, 55);
    store.Set(4395, -70);

    CHECK_EQ(store.Get(1519), 55);
    CHECK_EQ(store.Get(4395), -70);
}

TEST(FishingBaseSkillStore_SecondSetOverwrites)
{
    FishingBaseSkillStore store;
    store.Set(40, 100);
    store.Set(40, 205);

    CHECK_EQ(store.Get(40), 205);
}

TEST(FishingBaseSkillStore_ClearForgetsEveryArea)
{
    FishingBaseSkillStore store;
    store.Set(12, 25);
    store.Set(40, 100);
    store.Clear();

    CHECK_EQ(store.Get(12), 0);
    CHECK_EQ(store.Get(40), 0);

    store.Set(40, 330);
    CHECK_EQ(store.Get(40), 330);
    CHECK_EQ(store.Get(12), 0);
}
