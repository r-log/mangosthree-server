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

/// The gossip menus and their options, added and looked up with no database: a menu's texts and
/// options in the order added, a menu id with none, the menus walked in id order, and each part
/// cleared on its own.

#include "TestHarness.h"
#include "data/GossipMenuStore.h"

namespace
{
    GossipMenus MakeMenu(uint32 entry, uint32 textId)
    {
        GossipMenus menu = GossipMenus();
        menu.entry = entry;
        menu.text_id = textId;
        return menu;
    }

    GossipMenuItems MakeItem(uint32 menuId, uint32 id)
    {
        GossipMenuItems item = GossipMenuItems();
        item.menu_id = menuId;
        item.id = id;
        return item;
    }
}

TEST(GossipMenuStore_AMenuHoldsItsTextsInTheOrderAdded)
{
    GossipMenuStore store;
    CHECK(!store.HasMenu(50));

    store.AddMenu(MakeMenu(50, 7));
    store.AddMenu(MakeMenu(60, 9));
    store.AddMenu(MakeMenu(50, 8));
    CHECK(store.HasMenu(50));
    CHECK(!store.HasMenu(51));

    GossipMenusMapBounds menu = store.Menus(50);
    REQUIRE(menu.first != menu.second);
    CHECK_EQ(menu.first->second.text_id, uint32(7));
    ++menu.first;
    REQUIRE(menu.first != menu.second);
    CHECK_EQ(menu.first->second.text_id, uint32(8));
    ++menu.first;
    CHECK(menu.first == menu.second);

    GossipMenusMap::const_iterator first = store.AllMenus().begin();
    CHECK_EQ(first->first, uint32(50));
    CHECK_EQ(store.AllMenus().size(), size_t(3));
}

TEST(GossipMenuStore_AMenuHoldsItsOptionsInTheOrderAdded)
{
    GossipMenuStore store;
    store.AddMenuItem(MakeItem(0, 1));
    store.AddMenuItem(MakeItem(50, 3));
    store.AddMenuItem(MakeItem(50, 2));

    GossipMenuItemsMapBounds options = store.MenuItems(50);
    REQUIRE(options.first != options.second);
    CHECK_EQ(options.first->second.id, uint32(3));
    ++options.first;
    REQUIRE(options.first != options.second);
    CHECK_EQ(options.first->second.id, uint32(2));

    GossipMenuItemsMapBounds defaults = store.MenuItems(0);
    REQUIRE(defaults.first != defaults.second);
    CHECK_EQ(defaults.first->second.id, uint32(1));

    GossipMenuItemsMapBounds none = store.MenuItems(51);
    CHECK(none.first == none.second);
}

TEST(GossipMenuStore_EachPartClearsOnItsOwn)
{
    GossipMenuStore store;
    store.AddMenu(MakeMenu(50, 7));
    store.AddMenuItem(MakeItem(50, 3));

    store.ClearMenuItems();
    GossipMenuItemsMapBounds options = store.MenuItems(50);
    CHECK(options.first == options.second);
    CHECK(store.HasMenu(50));

    store.ClearMenus();
    CHECK(!store.HasMenu(50));
    CHECK(store.AllMenus().empty());
}
