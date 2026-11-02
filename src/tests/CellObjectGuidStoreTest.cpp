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

/// The spawn guids filed by where they load, with no database: a cell found or added by map, spawn
/// mode and cell id, the same cell id under another spawn mode or map, a read that adds nothing, and
/// the active creatures per map in the order added.

#include "TestHarness.h"
#include "data/CellObjectGuidStore.h"

TEST(CellObjectGuidStore_ACellIsAddedForAnEditAndFoundAgain)
{
    CellObjectGuidStore store;
    CHECK(store.FindCell(0, 0, 4000) == NULL);

    store.FindOrAddCell(0, 0, 4000).creatures.insert(10);
    store.FindOrAddCell(0, 0, 4000).gameobjects.insert(20);
    store.FindOrAddCell(0, 0, 4000).corpses[7] = 0;

    CellObjectGuids const* cell = store.FindCell(0, 0, 4000);
    REQUIRE(cell != NULL);
    CHECK_EQ(cell->creatures.count(10), size_t(1));
    CHECK_EQ(cell->gameobjects.count(20), size_t(1));
    CHECK_EQ(cell->corpses.count(7), size_t(1));
}

TEST(CellObjectGuidStore_TheSpawnModeAndTheMapAreEachPartOfTheKey)
{
    CellObjectGuidStore store;
    store.FindOrAddCell(533, 1, 4000).creatures.insert(10);

    CHECK(store.FindCell(533, 0, 4000) == NULL);
    CHECK(store.FindCell(532, 1, 4000) == NULL);
    CHECK(store.FindCell(533, 1, 4001) == NULL);
    REQUIRE(store.FindCell(533, 1, 4000) != NULL);
    CHECK_EQ(store.FindCell(533, 1, 4000)->creatures.size(), size_t(1));
}

TEST(CellObjectGuidStore_AReadAddsNoCell)
{
    CellObjectGuidStore store;
    CHECK(store.FindCell(1, 0, 10) == NULL);
    CHECK(store.FindCell(1, 0, 10) == NULL);

    CellObjectGuids& empty = store.FindOrAddCell(1, 0, 10);
    CHECK(empty.creatures.empty());
    CHECK(store.FindCell(1, 0, 10) != NULL);
}

TEST(CellObjectGuidStore_AMapHoldsItsActiveCreaturesInTheOrderAdded)
{
    CellObjectGuidStore store;
    store.AddActiveCreature(0, 300);
    store.AddActiveCreature(1, 301);
    store.AddActiveCreature(0, 302);

    typedef CellObjectGuidStore::ActiveCreatureGuidsOnMap Active;
    std::pair<Active::const_iterator, Active::const_iterator> map0 = store.ActiveCreatures().equal_range(0);
    REQUIRE(map0.first != map0.second);
    CHECK_EQ(map0.first->second, uint32(300));
    ++map0.first;
    REQUIRE(map0.first != map0.second);
    CHECK_EQ(map0.first->second, uint32(302));
    ++map0.first;
    CHECK(map0.first == map0.second);
    CHECK_EQ(store.ActiveCreatures().count(1), size_t(1));
}
