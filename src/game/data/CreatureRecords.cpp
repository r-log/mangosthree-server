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

#include "data/CreatureRecords.h"
#include "ObjectMgr.h"

ObjectGuid CreatureData::GetObjectGuid(uint32 lowguid) const
{
    // info existence checked at loading
    return ObjectMgr::GetCreatureTemplate(id)->GetObjectGuid(lowguid);
}

/**
 * @brief Finds a trainer spell entry by spell id.
 *
 * @param spell_id The spell identifier to look up.
 * @return The matching trainer spell, or null if not found.
 */
TrainerSpell const* TrainerSpellData::Find(uint32 spell_id) const
{
    TrainerSpellMap::const_iterator itr = spellList.find(spell_id);
    if (itr != spellList.end())
    {
        return &itr->second;
    }

    return NULL;
}

void VendorItemData::AddItem(uint32 item, uint8 type, uint32 maxcount, uint32 ptime, uint32 ExtendedCost, uint16 conditonId)
{
    m_items.push_back(new VendorItem(item, type, maxcount, ptime, ExtendedCost, conditonId));
}

/**
 * @brief Removes a vendor item by item id.
 *
 * @param item_id The item entry to remove.
 * @return true if an item was removed; otherwise, false.
 */
bool VendorItemData::RemoveItem(uint32 item_id, uint8 type)
{
    bool found = false;
    for (VendorItemList::iterator i = m_items.begin(); i != m_items.end();)
    {
        // can have many examples
        if ((*i)->item == item_id && (*i)->type == type)
        {
            i = m_items.erase(i);
            found = true;
        }
        else
        {
            ++i;
        }
    }

    return found;
}

/**
 * @brief Finds vendor item data by item id.
 *
 * @param item_id The item entry to search for.
 * @return The matching vendor item, or null if not found.
 */
VendorItem const* VendorItemData::FindItem(uint32 item_id) const
{
    for (VendorItemList::const_iterator i = m_items.begin(); i != m_items.end(); ++i)
    {
        // Skip checking for conditions, condition system is powerfull enough to not require additional entries only for the conditions
        if ((*i)->item == item_id)
        {
            return *i;
        }
    }
    return NULL;
}

VendorItem const* VendorItemData::FindItemCostPair(uint32 item_id, uint8 type, uint32 extendedCost) const
{
    for (VendorItemList::const_iterator i = m_items.begin(); i != m_items.end(); ++i)
    {
        // Skip checking for conditions, condition system is powerfull enough to not require additional entries only for the conditions
        if ((*i)->item == item_id && (*i)->ExtendedCost == extendedCost && (*i)->type == type)
        {
            return *i;
        }
    }
    return NULL;
}
