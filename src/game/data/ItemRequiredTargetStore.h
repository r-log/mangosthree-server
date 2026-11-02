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

#ifndef MANGOS_H_ITEMREQUIREDTARGETSTORE
#define MANGOS_H_ITEMREQUIREDTARGETSTORE

#include "Platform/Define.h"
#include "ItemPrototype.h"

#include <map>
#include <utility>

typedef std::multimap<uint32, ItemRequiredTarget> ItemRequiredTargetMap;
typedef std::pair<ItemRequiredTargetMap::const_iterator, ItemRequiredTargetMap::const_iterator> ItemRequiredTargetMapBounds;

/**
 * The item required targets (`item_required_target`): per item entry, the creatures an item's spell
 * may be used on, each alive or dead, in the order added. The loader clears and fills it.
 */
class ItemRequiredTargetStore
{
    public:
        void Clear() { m_targets.clear(); }
        void Add(uint32 itemEntry, ItemRequiredTarget const& target) { m_targets.insert(ItemRequiredTargetMap::value_type(itemEntry, target)); }
        ItemRequiredTargetMapBounds ForItem(uint32 itemEntry) const { return m_targets.equal_range(itemEntry); }

    private:
        ItemRequiredTargetMap m_targets;
};

#endif
