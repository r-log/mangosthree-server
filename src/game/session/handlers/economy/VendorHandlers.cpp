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

#include "session/handlers/economy/VendorHandlers.h"

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "WorldHandlers/AchievementMgr.h"
#include "entities/player/Player.h"
#include "Object/Item.h"
#include "Object/Bag.h"
#include "Object/Creature.h"
#include "Server/DBCStores.h"

/**
 * @brief Sells an item stack to a vendor.
 *
 * @param recv_data The received opcode packet.
 */
void VendorHandlers::HandleSellItemOpcode(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SELL_ITEM");

    ObjectGuid vendorGuid;
    ObjectGuid itemGuid;
    uint32 count;

    recv_data >> vendorGuid;
    recv_data >> itemGuid;
    recv_data >> count;

    if (!itemGuid)
    {
        return;
    }

    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleSellItemOpcode - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        session.GetPlayer()->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, itemGuid, 0);
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(itemGuid);
    if (pItem)
    {
        // prevent sell not owner item
        if (session.GetPlayer()->GetObjectGuid() != pItem->GetOwnerGuid())
        {
            session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // prevent sell non empty bag by drag-and-drop at vendor's item list
        if (pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // prevent sell currently looted item
        if (session.GetPlayer()->GetLootGuid() == pItem->GetObjectGuid())
        {
            session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // special case at auto sell (sell all)
        if (count == 0)
        {
            count = pItem->GetCount();
        }
        else
        {
            // prevent sell more items that exist in stack (possible only not from client)
            if (count > pItem->GetCount())
            {
                session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                return;
            }
        }

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pProto->SellPrice > 0)
            {
                uint64 money = pProto->SellPrice * count;
                if (session.GetPlayer()->GetMoney() >= MAX_MONEY_AMOUNT - money) // prevent exceeding gold limit
                {
                    session.GetPlayer()->SendEquipError(EQUIP_ERR_TOO_MUCH_GOLD, nullptr, nullptr);
                    session.GetPlayer()->SendSellError(SELL_ERR_UNK, pCreature, itemGuid, 0);
                    return;
                }

                if (count < pItem->GetCount())              // need split items
                {
                    Item* pNewItem = pItem->CloneItem(count, session.GetPlayer());
                    if (!pNewItem)
                    {
                        sLog.outError("WORLD: HandleSellItemOpcode - could not create clone of item %u; count = %u", pItem->GetEntry(), count);
                        session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                        return;
                    }

                    pItem->SetCount(pItem->GetCount() - count);
                    session.GetPlayer()->ItemRemovedQuestCheck(pItem->GetEntry(), count);
                    if (session.GetPlayer()->IsInWorld())
                    {
                        pItem->SendCreateUpdateToPlayer(session.GetPlayer());
                    }
                    pItem->SetState(ITEM_CHANGED, session.GetPlayer());

                    session.GetPlayer()->AddItemToBuyBackSlot(pNewItem);
                    if (session.GetPlayer()->IsInWorld())
                    {
                        pNewItem->SendCreateUpdateToPlayer(session.GetPlayer());
                    }
                }
                else
                {
                    session.GetPlayer()->ItemRemovedQuestCheck(pItem->GetEntry(), pItem->GetCount());
                    session.GetPlayer()->RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
                    pItem->RemoveFromUpdateQueueOf(session.GetPlayer());
                    session.GetPlayer()->AddItemToBuyBackSlot(pItem);
                }

                session.GetPlayer()->ModifyMoney(money);
                session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_MONEY_FROM_VENDORS, money);
            }
            else
            {
                session.GetPlayer()->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            }
            return;
        }
    }
    session.GetPlayer()->SendSellError(SELL_ERR_CANT_FIND_ITEM, pCreature, itemGuid, 0);
    return;
}

/**
 * @brief Buys back a previously sold item.
 *
 * @param recv_data The received opcode packet.
 */
void VendorHandlers::HandleBuybackItem(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUYBACK_ITEM");
    ObjectGuid vendorGuid;
    uint32 slot;

    recv_data >> vendorGuid >> slot;

    Creature* pCreature = session.GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleBuybackItem - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        session.GetPlayer()->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, ObjectGuid(), 0);
        return;
    }

    // remove fake death
    if (session.GetPlayer()->IsFeigningDeath())
    {
        session.GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = session.GetPlayer()->GetInventoryMgr().GetItemFromBuyBackSlot(slot);
    if (pItem)
    {
        uint64 price = session.GetPlayer()->GetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START);
        if (session.GetPlayer()->GetMoney() < price)
        {
            session.GetPlayer()->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }

        ItemPosCountVec dest;
        InventoryResult msg = session.GetPlayer()->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg == EQUIP_ERR_OK)
        {
            session.GetPlayer()->ModifyMoney(-(int64)price);
            session.GetPlayer()->RemoveItemFromBuyBackSlot(slot, false);
            session.GetPlayer()->ItemAddedQuestCheck(pItem->GetEntry(), pItem->GetCount());
            session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_RECEIVE_EPIC_ITEM, pItem->GetEntry(), pItem->GetCount());
            session.GetPlayer()->StoreItem(dest, pItem, true);
        }
        else
        {
            session.GetPlayer()->SendEquipError(msg, pItem, NULL);
        }
        return;
    }
    else
    {
        session.GetPlayer()->SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
    }
}

/**
 * @brief Buys an item from a vendor into automatic storage.
 *
 * @param recv_data The received opcode packet.
 */
void VendorHandlers::HandleBuyItemOpcode(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid vendorGuid, bagGuid;
    uint32 item, slot, count;
    uint8 type, bagSlot;

    recv_data >> vendorGuid >> type >> item >> slot >> count >> bagGuid >> bagSlot;
    DEBUG_LOG("WORLD: Received opcode CMSG_BUY_ITEM, vendorguid: %s, type: %u, item: %u, slot: %u, count: %u, bagGuid: %s, bagSlog: %u",
        vendorGuid.GetString().c_str(), type, item, slot, count, bagGuid.GetString().c_str(), bagSlot);

    // client side expected counting from 1, and we send to client vendorslot+1 already
    if (slot > 0)
    {
        --slot;
    }
    else
    {
        return;                                             // cheating
    }

    switch (type)
    {
        case VENDOR_ITEM_TYPE_NONE:
            break;
        case VENDOR_ITEM_TYPE_ITEM:
        {
            uint8 bag = NULL_BAG;                           // init for case invalid bagGUID

            // find bag slot by bag guid
            if (bagGuid == session.GetPlayer()->GetObjectGuid())
            {
                bag = INVENTORY_SLOT_BAG_0;
            }
            else
            {
                for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
                {
                    if (Bag* pBag = (Bag*)session.GetPlayer()->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, i))
                    {
                        if (bagGuid == pBag->GetObjectGuid())
                        {
                            bag = i;
                            break;
                        }
                    }
                }
            }

            session.GetPlayer()->BuyItemFromVendorSlot(vendorGuid, slot, item, count, bag, bagSlot);
            break;
        }
        case VENDOR_ITEM_TYPE_CURRENCY:
        {
            session.GetPlayer()->BuyCurrencyFromVendorSlot(vendorGuid, slot, item, count);
            break;
        }
    }
}

/**
 * @brief Requests the inventory list of a vendor.
 *
 * @param recv_data The received opcode packet.
 */
void VendorHandlers::HandleListInventoryOpcode(WorldSession& session, WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    if (!session.GetPlayer()->IsAlive())
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_LIST_INVENTORY");

    session.SendListInventory(guid);
}

/**
 * @brief Auto-stores an item into a destination bag.
 *
 * @param recv_data The received opcode packet.
 */
void VendorHandlers::HandleAutoStoreBagItemOpcode(WorldSession& session, WorldPacket& recv_data)
{
    // DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BAG_ITEM");
    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;
    // DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u", srcbag, srcslot, dstbag);

    Item* pItem = session.GetPlayer()->GetInventoryMgr().GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (!session.GetPlayer()->GetInventoryMgr().IsValidPos(dstbag, NULL_SLOT, false))     // can be autostore pos
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL);
        return;
    }

    uint16 src = pItem->GetPos();

    // check unequip potability for equipped items and bank bags
    if (session.GetPlayer()->GetInventoryMgr().IsEquipmentPos(src) || session.GetPlayer()->GetInventoryMgr().IsBagPos(src))
    {
        InventoryResult msg = session.GetPlayer()->CanUnequipItem(src, !session.GetPlayer()->GetInventoryMgr().IsBagPos(src));
        if (msg != EQUIP_ERR_OK)
        {
            session.GetPlayer()->SendEquipError(msg, pItem, NULL);
            return;
        }
    }

    ItemPosCountVec dest;
    InventoryResult msg = session.GetPlayer()->CanStoreItem(dstbag, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        session.GetPlayer()->SendEquipError(msg, pItem, NULL);
        return;
    }

    // no-op: placed in same slot
    if (dest.size() == 1 && dest[0].pos == src)
    {
        // just remove gray item state
        session.GetPlayer()->SendEquipError(EQUIP_ERR_NONE, pItem, NULL);
        return;
    }

    session.GetPlayer()->RemoveItem(srcbag, srcslot, true);
    session.GetPlayer()->StoreItem(dest, pItem, true);
}

/**
 * @brief Purchases the next available bank bag slot.
 *
 * @param recvPacket The received opcode packet.
 */
void VendorHandlers::HandleBuyBankSlotOpcode(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_BUY_BANK_SLOT");

    ObjectGuid guid;
    recvPacket >> guid;

    if (!session.CheckBanker(guid))
    {
        return;
    }

    uint32 slot = session.GetPlayer()->GetBankBagSlotCount();

    // next slot
    ++slot;

    DETAIL_LOG("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if (!slotEntry)
    {
        return;
    }

    uint64 price = slotEntry->Cost;

    if (session.GetPlayer()->GetMoney() < price)
    {
        return;
    }

    session.GetPlayer()->SetBankBagSlotCount(slot);
    session.GetPlayer()->ModifyMoney(-int64(price));

    session.GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_BUY_BANK_SLOT);
}

/**
 * @brief Moves an item from inventory into the bank automatically.
 *
 * @param recvPacket The received opcode packet.
 */
void VendorHandlers::HandleAutoBankItemOpcode(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = session.GetPlayer()->GetInventoryMgr().GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = session.GetPlayer()->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        session.GetPlayer()->SendEquipError(msg, pItem, NULL);
        return;
    }

    // no-op: placed in same slot
    if (dest.size() == 1 && dest[0].pos == pItem->GetPos())
    {
        // just remove gray item state
        session.GetPlayer()->SendEquipError(EQUIP_ERR_NONE, pItem, NULL);
        return;
    }

    session.GetPlayer()->RemoveItem(srcbag, srcslot, true);
    session.GetPlayer()->BankItem(dest, pItem, true);
}

/**
 * @brief Moves an item between bank and inventory automatically.
 *
 * @param recvPacket The received opcode packet.
 */
void VendorHandlers::HandleAutoStoreBankItemOpcode(WorldSession& session, WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = session.GetPlayer()->GetInventoryMgr().GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (session.GetPlayer()->GetInventoryMgr().IsBankPos(srcbag, srcslot))                // moving from bank to inventory
    {
        ItemPosCountVec dest;
        InventoryResult msg = session.GetPlayer()->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            session.GetPlayer()->SendEquipError(msg, pItem, NULL);
            return;
        }

        session.GetPlayer()->RemoveItem(srcbag, srcslot, true);
        session.GetPlayer()->StoreItem(dest, pItem, true);
    }
    else                                                    // moving from inventory to bank
    {
        ItemPosCountVec dest;
        InventoryResult msg = session.GetPlayer()->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            session.GetPlayer()->SendEquipError(msg, pItem, NULL);
            return;
        }

        session.GetPlayer()->RemoveItem(srcbag, srcslot, true);
        session.GetPlayer()->BankItem(dest, pItem, true);
    }
}
