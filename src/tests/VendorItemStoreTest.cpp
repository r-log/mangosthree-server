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

/// The vendor item lists, filled and looked up with no database: a key with no list, a list filled in
/// place, the template lists kept apart from the creature lists, an edit, and each side cleared on
/// its own.

#include "TestHarness.h"
#include "data/VendorItemStore.h"

TEST(VendorItemStore_AKeyWithNoListAnswersNull)
{
    VendorItemStore store;
    CHECK(store.Find(false, 1000) == NULL);
    CHECK(store.Find(true, 1000) == NULL);
    CHECK(store.FindForEdit(false, 1000) == NULL);
}

TEST(VendorItemStore_AListIsFilledInPlaceOnItsOwnSide)
{
    VendorItemStore store;
    store.FindOrAdd(false, 1000).AddItem(159, VENDOR_ITEM_TYPE_ITEM, 0, 0, 0, 0);
    store.FindOrAdd(false, 1000).AddItem(4540, VENDOR_ITEM_TYPE_ITEM, 5, 3600, 0, 0);
    store.FindOrAdd(true, 7).AddItem(2512, VENDOR_ITEM_TYPE_ITEM, 0, 0, 0, 0);

    VendorItemData const* vendor = store.Find(false, 1000);
    REQUIRE(vendor != NULL);
    CHECK_EQ(vendor->GetItemCount(), uint8(2));
    CHECK_EQ(vendor->GetItem(1)->maxcount, uint32(5));
    CHECK(store.Find(true, 1000) == NULL);
    REQUIRE(store.Find(true, 7) != NULL);
    CHECK_EQ(store.Templates().size(), size_t(1));

    VendorItemData* edit = store.FindForEdit(false, 1000);
    REQUIRE(edit != NULL);
    CHECK(edit->RemoveItem(159, VENDOR_ITEM_TYPE_ITEM));
    CHECK_EQ(store.Find(false, 1000)->GetItemCount(), uint8(1));
}

TEST(VendorItemStore_EachSideClearsOnItsOwn)
{
    VendorItemStore store;
    store.FindOrAdd(false, 1000).AddItem(159, VENDOR_ITEM_TYPE_ITEM, 0, 0, 0, 0);
    store.FindOrAdd(true, 7).AddItem(2512, VENDOR_ITEM_TYPE_ITEM, 0, 0, 0, 0);

    store.Clear(true);
    CHECK(store.Find(true, 7) == NULL);
    CHECK(store.Find(false, 1000) != NULL);

    store.Clear(false);
    CHECK(store.Find(false, 1000) == NULL);
}
