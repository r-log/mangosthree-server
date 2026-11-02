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

#ifndef MANGOS_H_GOSSIPMENUSTORE
#define MANGOS_H_GOSSIPMENUSTORE

#include "Platform/Define.h"

#include <map>
#include <string>
#include <utility>

struct GossipMenuItems
{
    uint32          menu_id;
    uint32          id;
    uint8           option_icon;
    std::string     option_text;
    uint32          option_id;
    uint32          npc_option_npcflag;
    int32           action_menu_id;
    uint32          action_poi_id;
    uint32          action_script_id;
    bool            box_coded;
    uint32          box_money;
    std::string     box_text;
    uint16          conditionId;
};

struct GossipMenus
{
    uint32          entry;
    uint32          text_id;
    uint32          script_id;
    uint16          conditionId;
};

typedef std::multimap<uint32, GossipMenus> GossipMenusMap;
typedef std::pair<GossipMenusMap::const_iterator, GossipMenusMap::const_iterator> GossipMenusMapBounds;
typedef std::multimap<uint32, GossipMenuItems> GossipMenuItemsMap;
typedef std::pair<GossipMenuItemsMap::const_iterator, GossipMenuItemsMap::const_iterator> GossipMenuItemsMapBounds;

/**
 * The gossip menus (`gossip_menu`) and their options (`gossip_menu_option`): per menu id, the texts
 * the menu may show and the options it offers, each in the order added; the options of menu id 0
 * are the default ones. Each part is cleared and filled by its loader.
 */
class GossipMenuStore
{
    public:
        void ClearMenus() { m_menus.clear(); }
        void AddMenu(GossipMenus const& menu) { m_menus.insert(GossipMenusMap::value_type(menu.entry, menu)); }
        bool HasMenu(uint32 menuId) const { return m_menus.find(menuId) != m_menus.end(); }
        GossipMenusMapBounds Menus(uint32 menuId) const { return m_menus.equal_range(menuId); }
        GossipMenusMap const& AllMenus() const { return m_menus; }

        void ClearMenuItems() { m_menuItems.clear(); }
        void AddMenuItem(GossipMenuItems const& item) { m_menuItems.insert(GossipMenuItemsMap::value_type(item.menu_id, item)); }
        GossipMenuItemsMapBounds MenuItems(uint32 menuId) const { return m_menuItems.equal_range(menuId); }

    private:
        GossipMenusMap m_menus;
        GossipMenuItemsMap m_menuItems;
};

#endif
