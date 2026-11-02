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

#include "LootPackets.h"
#include "ByteBuffer.h"
#include "LootMgr.h"
#include "ObjectMgr.h"
#include "Player.h"

/**
 * @brief Serializes a loot item entry into a byte buffer.
 *
 * @param b The output byte buffer.
 * @param li The loot item view.
 * @return ByteBuffer& The populated byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootItem const& li)
{
    if (li.type == LOOT_ITEM_TYPE_ITEM)
    {
        b << uint32(li.itemid);
        b << uint32(li.count);                                  // nr of items of this type
        b << uint32(ObjectMgr::GetItemPrototype(li.itemid)->DisplayInfoID);
        b << uint32(li.randomSuffix);
        b << uint32(li.randomPropertyId);
    }
    else if (li.type == LOOT_ITEM_TYPE_CURRENCY)
    {
        b << uint32(li.itemid);
        b << uint32(li.count);
    }
    // b << uint8(0);                                       // slot type - will send after this function call
    return b;
}

/**
 * @brief Serializes a loot view for a specific viewer into a byte buffer.
 *
 * @param b The output byte buffer.
 * @param lv The loot view to serialize.
 * @return ByteBuffer& The populated byte buffer.
 */
ByteBuffer& operator<<(ByteBuffer& b, LootView const& lv)
{
    if (lv.permission == NONE_PERMISSION)
    {
        b << uint32(0);                                     // gold
        b << uint8(0);                                      // item count
        b << uint8(0);                                      // currency count
        return b;
    }

    Loot& l = lv.loot;

    uint8 itemsShown = 0;
    uint8 currenciesShown = 0;

    // gold
    b << uint32(l.gold);

    size_t count_pos = b.wpos();                            // pos of item count byte
    b << uint8(0);                                          // item count placeholder
    size_t currency_count_pos = b.wpos();                   // pos of currency count byte
    b << uint8(0);                                          // currency count placeholder

    for (uint8 i = 0; i < l.items.size(); ++i)
    {
        LootSlotType slot_type = l.items[i].GetSlotTypeForSharedLoot(lv.permission, lv.viewer, l.GetLootTarget());
        if (slot_type >= MAX_LOOT_SLOT_TYPE)
        {
            continue;
        }

        b << uint8(i) << l.items[i];
        b << uint8(slot_type);                              // 0 - get 1 - look only 2 - master selection
        ++itemsShown;
    }

    QuestItemMap const& lootPlayerNonQuestNonFFAConditionalItems = l.GetPlayerNonQuestNonFFANonCurrencyConditionalItems();
    QuestItemMap::const_iterator nn_itr = lootPlayerNonQuestNonFFAConditionalItems.find(lv.viewer->GetGUIDLow());
    if (nn_itr != lootPlayerNonQuestNonFFAConditionalItems.end())
    {
        QuestItemList* conditional_list =  nn_itr->second;
        for (QuestItemList::const_iterator ci = conditional_list->begin() ; ci != conditional_list->end(); ++ci)
        {
            LootItem& item = l.items[ci->index];

            LootSlotType slot_type = item.GetSlotTypeForSharedLoot(lv.permission, lv.viewer, l.GetLootTarget(), !ci->is_looted);
            if (slot_type >= MAX_LOOT_SLOT_TYPE)
            {
                continue;
            }

            b << uint8(ci->index) << item;
            b << uint8(slot_type);                          // allow loot
            ++itemsShown;
        }
    }

    // in next cases used same slot type for all items
    LootSlotType slot_type = lv.permission == OWNER_PERMISSION ? LOOT_SLOT_OWNER : LOOT_SLOT_NORMAL;

    QuestItemMap const& lootPlayerQuestItems = l.GetPlayerQuestItems();
    QuestItemMap::const_iterator q_itr = lootPlayerQuestItems.find(lv.viewer->GetGUIDLow());
    if (q_itr != lootPlayerQuestItems.end())
    {
        QuestItemList* q_list = q_itr->second;
        for (QuestItemList::const_iterator qi = q_list->begin() ; qi != q_list->end(); ++qi)
        {
            LootItem const& item = l.GetQuestItems()[qi->index];
            if (!qi->is_looted && !item.is_looted)
            {
                b << uint8(l.items.size() + (qi - q_list->begin()));
                b << item;
                b << uint8(slot_type);                      // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerFFAItems = l.GetPlayerFFAItems();
    QuestItemMap::const_iterator ffa_itr = lootPlayerFFAItems.find(lv.viewer->GetGUIDLow());
    if (ffa_itr != lootPlayerFFAItems.end())
    {
        QuestItemList* ffa_list = ffa_itr->second;
        for (QuestItemList::const_iterator fi = ffa_list->begin() ; fi != ffa_list->end(); ++fi)
        {
            LootItem& item = l.items[fi->index];
            if (!fi->is_looted && !item.is_looted)
            {
                b << uint8(fi->index) << item;
                b << uint8(slot_type);                      // allow loot
                ++itemsShown;
            }
        }
    }

    QuestItemMap const& lootPlayerCurrencies = l.GetPlayerCurrencies();
    QuestItemMap::const_iterator currency_itr = lootPlayerCurrencies.find(lv.viewer->GetGUIDLow());
    if (currency_itr != lootPlayerCurrencies.end())
    {
        QuestItemList* currency_list = currency_itr->second;
        for (QuestItemList::const_iterator ci = currency_list->begin() ; ci != currency_list->end(); ++ci)
        {
            LootItem& item = l.items[ci->index];
            if (!ci->is_looted && !item.is_looted)
            {
                b << uint8(ci->index) << item;
                ++currenciesShown;
            }
        }
    }

    // update number of items and currencies shown
    b.put<uint8>(count_pos, itemsShown);
    b.put<uint8>(currency_count_pos, currenciesShown);

    return b;
}
