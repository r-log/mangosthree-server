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

/**
 * @file PlayerItem.cpp
 * @brief Player's equip-slot lookup, the equipped item and gem queries, QuickEquipItem and TradeCancel.
 */

#include "Player.h"
#include "data/Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GridNotifiersImpl.h"
#include "ObjectMgr.h"
#include "Pet.h"
#include "Util.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "Vehicle.h"
#include "DisableMgr.h"

/*********************************************************/
/***                    STORAGE SYSTEM                 ***/
/*********************************************************/

uint8 Player::FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const
{
    uint8 slots[4];
    if (!GetSlotsForInventoryType(proto->InventoryType, slots, proto->SubClass))
    {
        return NULL_SLOT;
    }

    if (slot != NULL_SLOT)
    {
        if (swap || !m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            for (int i = 0; i < 4; ++i)
            {
                if (slots[i] == slot)
                {
                    return slot;
                }
            }
        }
    }
    else
    {
        // search free slot at first
        for (int i = 0; i < 4; ++i)
        {
            if (slots[i] != NULL_SLOT && !m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, slots[i]))
            {
                // in case 2hand equipped weapon (without titan grip) offhand slot empty but not free
                if (slots[i] != EQUIPMENT_SLOT_OFFHAND || !IsTwoHandUsed())
                {
                    return slots[i];
                }
            }
        }

        // if not found free and can swap return first appropriate from used
        for (int i = 0; i < 4; ++i)
        {
            if (slots[i] != NULL_SLOT && swap)
            {
                return slots[i];
            }
        }
    }

    // no free position
    return NULL_SLOT;
}

bool Player::HasItemOrGemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == int(except_slot))
        {
            continue;
        }

        Item* pItem = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return true;
            }
        }
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(item);
    if (pProto && pProto->GemProperties)
    {
        for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (i == int(except_slot))
            {
                continue;
            }

            Item* pItem = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem->GetProto()->Socket[0].Color)
            {
                tempcount += pItem->GetGemCountWithID(item);
                if (tempcount >= count)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool Player::HasItemOrGemWithLimitCategoryEquipped(uint32 limitCategory, uint32 count, uint8 except_slot) const
{
    uint32 tempcount = 0;
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (i == int(except_slot))
        {
            continue;
        }

        Item* pItem = m_inventoryMgr.GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (!pItem)
        {
            continue;
        }

        ItemPrototype const* pProto = pItem->GetProto();
        if (!pProto)
        {
            continue;
        }

        if (pProto->ItemLimitCategory == limitCategory)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return true;
            }
        }

        if (pProto->Socket[0].Color)
        {
            tempcount += pItem->GetGemCountWithLimitCategory(limitCategory);
            if (tempcount >= count)
            {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Equips an item without full validation, typically during loading.
 *
 * @param pos The packed destination position.
 * @param pItem The item to equip.
 */
void Player::QuickEquipItem(uint16 pos, Item* pItem)
{
    if (pItem)
    {
        AddEnchantmentDurations(pItem);
        AddItemDurations(pItem);
        ApplyItemOnStoreSpell(pItem, true);

        uint8 slot = pos & 255;
        VisualizeItem(slot, pItem);

        if (IsInWorld())
        {
            pItem->AddToWorld();
            pItem->SendCreateUpdateToPlayer(this);
        }
        // Apply Titan's Grip damage penalty if necessary
        if ((slot == EQUIPMENT_SLOT_MAINHAND || slot == EQUIPMENT_SLOT_OFFHAND) && CanTitanGrip() && HasTwoHandWeaponInOneHand() && !HasAura(49152))
        {
            CastSpell(this, 49152, true);
        }

        GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EQUIP_ITEM, pItem->GetEntry());
        GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM, slot + 1);

        UpdateArmorSpecializations();
    }
}

/**
 * @brief Cancels the current trade and clears trade state for both players.
 *
 * @param sendback True to send a cancel notification to this player as well.
 */
void Player::TradeCancel(bool sendback)
{
    if (m_trade)
    {
        Player* trader = m_trade->GetTrader();

        // send yellow "Trade canceled" message to both traders
        if (sendback)
        {
            GetSession()->SendCancelTrade();
        }

        if (trader)
        {
            trader->GetSession()->SendCancelTrade();

            // cleanup trader's trade data
            delete trader->m_trade;
            trader->m_trade = NULL;
        }

        delete m_trade;
        m_trade = NULL;
    }
}

