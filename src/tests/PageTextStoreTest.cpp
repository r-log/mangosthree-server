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

/// The page texts, set and looked up with no database: an id with no page, an id with one (every
/// field), the store's own copy of the text, a second page for the same id, the highest id, an
/// edited next page, and a cleared store.

#include "TestHarness.h"
#include "data/PageTextStore.h"

#include <string>

TEST(PageTextStore_IdWithNoPageAnswersNull)
{
    PageTextStore store;
    CHECK(store.Find(7) == NULL);
    CHECK(store.FindForEdit(7) == NULL);

    store.Set(7, "Page seven", 0);
    CHECK(store.Find(7) != NULL);
    CHECK(store.Find(6) == NULL);
    CHECK(store.Find(8) == NULL);
}

TEST(PageTextStore_IdWithPageAnswersItsFields)
{
    PageTextStore store;
    store.Set(42, "The first page.", 43);

    PageText const* page = store.Find(42);
    REQUIRE(page != NULL);
    CHECK_EQ(page->Page_ID, 42u);
    CHECK_STR(std::string(page->Text), "The first page.");
    CHECK_EQ(page->Next_Page, 43u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(PageTextStore_KeepsItsOwnCopyOfTheText)
{
    PageTextStore store;
    std::string text = "Written once";
    store.Set(3, text, 0);
    text = "Changed after";

    PageText const* page = store.Find(3);
    REQUIRE(page != NULL);
    CHECK_STR(std::string(page->Text), "Written once");

    store.Set(4, "", 0);
    PageText const* empty = store.Find(4);
    REQUIRE(empty != NULL);
    REQUIRE(empty->Text != NULL);
    CHECK_STR(std::string(empty->Text), "");
}

TEST(PageTextStore_SecondSetOverwrites)
{
    PageTextStore store;
    store.Set(5, "Old", 6);
    store.Set(5, "New", 9);

    PageText const* page = store.Find(5);
    REQUIRE(page != NULL);
    CHECK_STR(std::string(page->Text), "New");
    CHECK_EQ(page->Next_Page, 9u);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(PageTextStore_MaxEntryIsOnePastTheHighestId)
{
    PageTextStore store;
    CHECK_EQ(store.GetMaxEntry(), 0u);

    store.Set(10, "Ten", 0);
    store.Set(2, "Two", 0);
    CHECK_EQ(store.GetMaxEntry(), 11u);

    store.Set(0, "Zero", 0);
    CHECK_EQ(store.GetMaxEntry(), 11u);
    CHECK(store.Find(0) != NULL);
}

TEST(PageTextStore_EditChangesTheStoredPage)
{
    PageTextStore store;
    store.Set(1, "One", 2);
    store.Set(2, "Two", 1);

    PageText* page = store.FindForEdit(2);
    REQUIRE(page != NULL);
    page->Next_Page = 0;

    CHECK_EQ(store.Find(2)->Next_Page, 0u);
    CHECK_EQ(store.Find(1)->Next_Page, 2u);
}

TEST(PageTextStore_ClearForgetsEveryId)
{
    PageTextStore store;
    store.Set(1, "One", 0);
    store.Set(2, "Two", 0);
    store.Clear();

    CHECK(store.Find(1) == NULL);
    CHECK(store.Find(2) == NULL);
    CHECK_EQ(store.GetCount(), 0u);
    CHECK_EQ(store.GetMaxEntry(), 0u);

    store.Set(2, "Again", 0);
    CHECK(store.Find(2) != NULL);
    CHECK(store.Find(1) == NULL);
}
