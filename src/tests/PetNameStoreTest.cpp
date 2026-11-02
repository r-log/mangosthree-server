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

/// The pet name parts, with no database: an entry with none, each half in the order added, and the
/// two halves and the entries kept apart.

#include "TestHarness.h"
#include "data/PetNameStore.h"

TEST(PetNameStore_AnEntryWithNoPartsAnswersEmptyLists)
{
    PetNameStore store;
    CHECK(store.FirstHalves(416).empty());
    CHECK(store.SecondHalves(416).empty());
}

TEST(PetNameStore_EachHalfHoldsItsWordsInTheOrderAdded)
{
    PetNameStore store;
    store.AddFirstHalf(416, "Bel");
    store.AddSecondHalf(416, "ruk");
    store.AddFirstHalf(416, "Gor");
    store.AddFirstHalf(1860, "Zal");

    REQUIRE(store.FirstHalves(416).size() == size_t(2));
    CHECK_STR(store.FirstHalves(416)[0], "Bel");
    CHECK_STR(store.FirstHalves(416)[1], "Gor");
    REQUIRE(store.SecondHalves(416).size() == size_t(1));
    CHECK_STR(store.SecondHalves(416)[0], "ruk");
    CHECK_EQ(store.FirstHalves(1860).size(), size_t(1));
    CHECK(store.SecondHalves(1860).empty());
}
