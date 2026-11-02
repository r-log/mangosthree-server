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

/// The pet stats per level, with no database: an entry with none, an entry's array added once and
/// filled in place, a second add that keeps it, and every entry walked in entry order.

#include "TestHarness.h"
#include "data/PetLevelInfoStore.h"

TEST(PetLevelInfoStore_AnEntryWithNoLevelsAnswersNull)
{
    PetLevelInfoStore store;
    CHECK(store.Levels(416) == NULL);
    CHECK(store.All().empty());
}

TEST(PetLevelInfoStore_AnEntrysLevelsAreAddedOnceAndFilledInPlace)
{
    PetLevelInfoStore store;
    PetLevelInfo* levels = store.FindOrAddLevels(416, 85);
    REQUIRE(levels != NULL);
    CHECK_EQ(levels[0].health, uint16(0));
    CHECK_EQ(levels[84].armor, uint16(0));
    levels[0].health = 50;
    levels[1].stats[STAT_STAMINA] = 7;

    CHECK(store.FindOrAddLevels(416, 85) == levels);
    PetLevelInfo const* found = store.Levels(416);
    REQUIRE(found != NULL);
    CHECK_EQ(found[0].health, uint16(50));
    CHECK_EQ(found[1].stats[STAT_STAMINA], uint16(7));
    CHECK(store.Levels(417) == NULL);
}

TEST(PetLevelInfoStore_EveryEntryIsWalkedInEntryOrder)
{
    PetLevelInfoStore store;
    store.FindOrAddLevels(1860, 2);
    store.FindOrAddLevels(416, 2);

    PetLevelInfoStore::PetLevelInfoMap::const_iterator itr = store.All().begin();
    REQUIRE(itr != store.All().end());
    CHECK_EQ(itr->first, uint32(416));
    ++itr;
    REQUIRE(itr != store.All().end());
    CHECK_EQ(itr->first, uint32(1860));
    ++itr;
    CHECK(itr == store.All().end());
}
