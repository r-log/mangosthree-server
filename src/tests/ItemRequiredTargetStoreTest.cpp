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

/// The item required targets, added and looked up with no database: an item's targets in the order
/// added, an item with none, and a cleared store.

#include "TestHarness.h"
#include "data/ItemRequiredTargetStore.h"

TEST(ItemRequiredTargetStore_AnItemHoldsItsTargetsInTheOrderAdded)
{
    ItemRequiredTargetStore store;
    store.Add(23337, ItemRequiredTarget(ITEM_TARGET_TYPE_CREATURE, 16880));
    store.Add(23338, ItemRequiredTarget(ITEM_TARGET_TYPE_DEAD, 17000));
    store.Add(23337, ItemRequiredTarget(ITEM_TARGET_TYPE_DEAD, 16881));

    ItemRequiredTargetMapBounds targets = store.ForItem(23337);
    REQUIRE(targets.first != targets.second);
    CHECK(targets.first->second.m_uiType == ITEM_TARGET_TYPE_CREATURE);
    CHECK_EQ(targets.first->second.m_uiTargetEntry, uint32(16880));
    ++targets.first;
    REQUIRE(targets.first != targets.second);
    CHECK(targets.first->second.m_uiType == ITEM_TARGET_TYPE_DEAD);
    CHECK_EQ(targets.first->second.m_uiTargetEntry, uint32(16881));
    ++targets.first;
    CHECK(targets.first == targets.second);

    ItemRequiredTargetMapBounds none = store.ForItem(1);
    CHECK(none.first == none.second);
}

TEST(ItemRequiredTargetStore_ClearingForgetsEveryItem)
{
    ItemRequiredTargetStore store;
    store.Add(23337, ItemRequiredTarget(ITEM_TARGET_TYPE_CREATURE, 16880));
    store.Clear();

    ItemRequiredTargetMapBounds targets = store.ForItem(23337);
    CHECK(targets.first == targets.second);
}
