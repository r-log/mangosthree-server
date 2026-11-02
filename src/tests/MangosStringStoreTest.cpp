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

/// The server strings and script texts, with no database: an entry with none, an entry filled in
/// place, a range erased while the entries outside it stay, negative entries, and the loaded counts
/// by the lowest entry of a range.

#include "TestHarness.h"
#include "data/MangosStringStore.h"

TEST(MangosStringStore_AnEntryWithNoStringAnswersNull)
{
    MangosStringStore store;
    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(-1000001) == NULL);
}

TEST(MangosStringStore_AnEntryIsFilledInPlace)
{
    MangosStringStore store;
    MangosStringLocale& data = store.FindOrAdd(-1000001);
    CHECK(data.Content.empty());
    CHECK(data.LanguageId == LANG_UNIVERSAL);
    data.Content.push_back("You will not defeat me!");
    data.Type = 1;

    MangosStringLocale const* found = store.Find(-1000001);
    REQUIRE(found != NULL);
    REQUIRE(found->Content.size() == size_t(1));
    CHECK_STR(found->Content[0], "You will not defeat me!");
    CHECK_EQ(found->Type, uint8(1));
}

TEST(MangosStringStore_ARangeIsErasedAndTheRestStays)
{
    MangosStringStore store;
    store.FindOrAdd(1).Content.push_back("one");
    store.FindOrAdd(5).Content.push_back("five");
    store.FindOrAdd(10).Content.push_back("ten");
    store.FindOrAdd(-1000).Content.push_back("negative");

    store.EraseRange(5, 10);
    CHECK(store.Find(5) == NULL);
    CHECK(store.Find(1) != NULL);
    CHECK(store.Find(10) != NULL);
    CHECK(store.Find(-1000) != NULL);

    store.EraseRange(-1999, -999);
    CHECK(store.Find(-1000) == NULL);
}

TEST(MangosStringStore_ALoadedCountIsKeptByTheLowestEntry)
{
    MangosStringStore store;
    CHECK_EQ(store.LoadedCount(1), uint32(0));

    store.SetLoadedCount(1, 1400);
    store.SetLoadedCount(-1000000, 20);
    CHECK_EQ(store.LoadedCount(1), uint32(1400));
    CHECK_EQ(store.LoadedCount(-1000000), uint32(20));
    CHECK_EQ(store.LoadedCount(2), uint32(0));

    store.SetLoadedCount(1, 1401);
    CHECK_EQ(store.LoadedCount(1), uint32(1401));
}
