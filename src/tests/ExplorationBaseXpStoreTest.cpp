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

/// The exploration base XP, set and looked up with no database: a level with no value, a level
/// with one, a second value for the same level, and two levels side by side.

#include "TestHarness.h"
#include "data/ExplorationBaseXpStore.h"

TEST(ExplorationBaseXpStore_LevelWithNoValueAnswersZero)
{
    ExplorationBaseXpStore store;
    CHECK_EQ(store.Get(10), 0u);

    store.Set(10, 125);
    CHECK_EQ(store.Get(9), 0u);
    CHECK_EQ(store.Get(11), 0u);
}

TEST(ExplorationBaseXpStore_LevelWithValueAnswersIt)
{
    ExplorationBaseXpStore store;
    store.Set(30, 450);

    CHECK_EQ(store.Get(30), 450u);
}

TEST(ExplorationBaseXpStore_SecondSetOverwrites)
{
    ExplorationBaseXpStore store;
    store.Set(20, 300);
    store.Set(20, 310);

    CHECK_EQ(store.Get(20), 310u);
}

TEST(ExplorationBaseXpStore_LevelsStayIndependent)
{
    ExplorationBaseXpStore store;
    store.Set(1, 5);
    store.Set(80, 1000);

    CHECK_EQ(store.Get(1), 5u);
    CHECK_EQ(store.Get(80), 1000u);

    store.Set(1, 6);
    CHECK_EQ(store.Get(1), 6u);
    CHECK_EQ(store.Get(80), 1000u);
}
