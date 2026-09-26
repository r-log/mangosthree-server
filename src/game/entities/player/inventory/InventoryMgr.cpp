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

#include "InventoryMgr.h"
#include "Bag.h"
#include "Item.h"
#include "Log.h"

#include <cstring>

// Every body below moved here in decoupling D4e1 from the character object's inventory files
// (PlayerItem.cpp, PlayerItemQuery.cpp, PlayerItemStorage.cpp), verbatim: the slot array kept
// its name, so each body reads the same slots in the same order and calls the same item and
// bag methods. The position arithmetic was static there and is static here; the buyback
// lookup, which only reads the array, is now const.

InventoryMgr::InventoryMgr()
{
    std::memset(m_items, 0, sizeof(Item*) * PLAYER_SLOTS_COUNT);
}

/**
 * @brief Checks whether a bag and slot refer to normal inventory storage.
 *
 * @param bag The bag identifier.
 * @param slot The slot identifier.
 * @return True if the position is an inventory position; otherwise, false.
 */
bool InventoryMgr::IsInventoryPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && slot == NULL_SLOT)
    {
        return true;
    }
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END))
    {
        return true;
    }
    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
    {
        return true;
    }
    return false;
}

/**
 * @brief Checks whether a bag and slot refer to an equipment position.
 *
 * @param bag The bag identifier.
 * @param slot The slot identifier.
 * @return True if the position is an equipment position; otherwise, false.
 */
bool InventoryMgr::IsEquipmentPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && (slot < EQUIPMENT_SLOT_END))
    {
        return true;
    }
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END))
    {
        return true;
    }
    return false;
}

/**
 * @brief Checks whether a bag and slot refer to bank storage.
 *
 * @param bag The bag identifier.
 * @param slot The slot identifier.
 * @return True if the position is in bank storage; otherwise, false.
 */
bool InventoryMgr::IsBankPos(uint8 bag, uint8 slot)
{
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END))
    {
        return true;
    }
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END))
    {
        return true;
    }
    if (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END)
    {
        return true;
    }
    return false;
}

/**
 * @brief Checks whether a packed position refers to a bag slot.
 *
 * @param pos The packed bag and slot position.
 * @return True if the position is a bag slot; otherwise, false.
 */
bool InventoryMgr::IsBagPos(uint16 pos)
{
    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END))
    {
        return true;
    }
    if (bag == INVENTORY_SLOT_BAG_0 && (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END))
    {
        return true;
    }
    return false;
}

/**
 * @brief Checks whether a bag and slot form a valid character storage position.
 *
 * @param bag The bag identifier.
 * @param slot The slot identifier.
 * @param explicit_pos True if the caller requires an explicit fixed position.
 * @return True if the position is valid; otherwise, false.
 */
bool InventoryMgr::IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const
{
    // post selected
    if (bag == NULL_BAG && !explicit_pos)
    {
        return true;
    }

    if (bag == INVENTORY_SLOT_BAG_0)
    {
        // any post selected
        if (slot == NULL_SLOT && !explicit_pos)
        {
            return true;
        }

        // equipment
        if (slot < EQUIPMENT_SLOT_END)
        {
            return true;
        }

        // bag equip slots
        if (slot >= INVENTORY_SLOT_BAG_START && slot < INVENTORY_SLOT_BAG_END)
        {
            return true;
        }

        // backpack slots
        if (slot >= INVENTORY_SLOT_ITEM_START && slot < INVENTORY_SLOT_ITEM_END)
        {
            return true;
        }

        // bank main slots
        if (slot >= BANK_SLOT_ITEM_START && slot < BANK_SLOT_ITEM_END)
        {
            return true;
        }

        // bank bag slots
        if (slot >= BANK_SLOT_BAG_START && slot < BANK_SLOT_BAG_END)
        {
            return true;
        }

        return false;
    }

    // bag content slots
    if (bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
        {
            return false;
        }

        // any post selected
        if (slot == NULL_SLOT && !explicit_pos)
        {
            return true;
        }

        return slot < pBag->GetBagSize();
    }

    // bank bag content slots
    if (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (!pBag)
        {
            return false;
        }

        // any post selected
        if (slot == NULL_SLOT && !explicit_pos)
        {
            return true;
        }

        return slot < pBag->GetBagSize();
    }

    // where this?
    return false;
}

/**
 * @brief Gets an item from a packed inventory position.
 *
 * @param pos The packed bag and slot position.
 * @return The item at that position, or null if empty.
 */
Item* InventoryMgr::GetItemByPos(uint16 pos) const
{
    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;
    return GetItemByPos(bag, slot);
}

/**
 * @brief Gets an item from a specific bag and slot location.
 *
 * @param bag The bag identifier.
 * @param slot The slot within the bag or inventory.
 * @return The item at that position, or null if empty.
 */
Item* InventoryMgr::GetItemByPos(uint8 bag, uint8 slot) const
{
    if (bag == INVENTORY_SLOT_BAG_0 && slot < BANK_SLOT_BAG_END)
    {
        return m_items[slot];
    }
    else if ((bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END)
             || (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END))
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
        {
            return pBag->GetItemByPos(slot);
        }
    }
    return NULL;
}

/**
 * @brief Finds an owned item by GUID across inventory and bank storage.
 *
 * @param guid The item GUID to search for.
 * @return The matching item, or null if none is found.
 */
Item* InventoryMgr::GetItemByGuid(ObjectGuid guid) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetObjectGuid() == guid)
            {
                return pItem;
            }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->GetObjectGuid() == guid)
                    {
                        return pItem;
                    }

    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetObjectGuid() == guid)
            {
                return pItem;
            }

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                if (Item* pItem = pBag->GetItemByPos(j))
                    if (pItem->GetObjectGuid() == guid)
                    {
                        return pItem;
                    }

    return NULL;
}

/**
 * @brief Finds the first owned item matching an entry identifier.
 *
 * @param item The item entry to search for.
 * @return The first matching item, or null if none is found.
 */
Item* InventoryMgr::GetItemByEntry(uint32 item) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetEntry() == item)
            {
                return pItem;
            }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (Item* itemPtr = pBag->GetItemByEntry(item))
            {
                return itemPtr;
            }

    return NULL;
}

Item* InventoryMgr::GetItemByLimitedCategory(uint32 limitedCategory) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetProto()->ItemLimitCategory == limitedCategory)
            {
                return pItem;
            }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (Item* itemPtr = pBag->GetItemByLimitedCategory(limitedCategory))
            {
                return itemPtr;
            }

    return NULL;
}

uint32 InventoryMgr::GetItemDisplayIdInSlot(uint8 bag, uint8 slot) const
{
    const Item* pItem = GetItemByPos(bag, slot);

    if (!pItem)
    {
        return 0;
    }

    return pItem->GetProto()->DisplayInfoID;
}

/**
 * @brief Gets an item from a vendor buyback slot.
 *
 * @param slot The buyback slot index.
 * @return The item in the slot, or null if none exists.
 */
Item* InventoryMgr::GetItemFromBuyBackSlot(uint32 slot) const
{
    DEBUG_LOG("STORAGE: GetItemFromBuyBackSlot slot = %u", slot);
    if (slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END)
    {
        return m_items[slot];
    }
    return NULL;
}

/**
 * @brief Counts matching items owned by the character.
 *
 * @param item The item entry to count.
 * @param inBankAlso True to include bank storage.
 * @param skipItem An item instance to exclude from the count.
 * @return The total matching item count.
 */
uint32 InventoryMgr::GetItemCount(uint32 item, bool inBankAlso, Item* skipItem) const
{
    uint32 count = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem != skipItem &&  pItem->GetEntry() == item)
        {
            count += pItem->GetCount();
        }
    }
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
        {
            count += pBag->GetItemCount(item, skipItem);
        }
    }

    if (skipItem && skipItem->GetProto()->GemProperties)
    {
        for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem != skipItem && pItem->GetProto()->Socket[0].Color)
            {
                count += pItem->GetGemCountWithID(item);
            }
        }
    }

    if (inBankAlso)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem != skipItem && pItem->GetEntry() == item)
            {
                count += pItem->GetCount();
            }
        }
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pBag)
            {
                count += pBag->GetItemCount(item, skipItem);
            }
        }

        if (skipItem && skipItem->GetProto()->GemProperties)
        {
            for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
            {
                Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
                if (pItem && pItem != skipItem && pItem->GetProto()->Socket[0].Color)
                {
                    count += pItem->GetGemCountWithID(item);
                }
            }
        }
    }

    return count;
}

uint32 InventoryMgr::GetItemCountWithLimitCategory(uint32 limitCategory, Item* skipItem) const
{
    uint32 count = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetProto()->ItemLimitCategory == limitCategory && pItem != skipItem)
            {
                count += pItem->GetCount();
            }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            count += pBag->GetItemCountWithLimitCategory(limitCategory, skipItem);
        }

    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            if (pItem->GetProto()->ItemLimitCategory == limitCategory && pItem != skipItem)
            {
                count += pItem->GetCount();
            }

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            count += pBag->GetItemCountWithLimitCategory(limitCategory, skipItem);
        }

    return count;
}

/**
 * @brief Checks whether the character owns at least a given count of an item.
 *
 * @param item The item entry to count.
 * @param count The required quantity.
 * @param inBankAlso True to include bank storage in the search.
 * @return True if enough items are present; otherwise, false.
 */
bool InventoryMgr::HasItemCount(uint32 item, uint32 count, bool inBankAlso) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return true;
            }
        }
    }
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                Item* pItem = GetItemByPos(i, j);
                if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    tempcount += pItem->GetCount();
                    if (tempcount >= count)
                    {
                        return true;
                    }
                }
            }
        }
    }

    if (inBankAlso)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                tempcount += pItem->GetCount();
                if (tempcount >= count)
                {
                    return true;
                }
            }
        }
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    Item* pItem = GetItemByPos(i, j);
                    if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
                    {
                        tempcount += pItem->GetCount();
                        if (tempcount >= count)
                        {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}
