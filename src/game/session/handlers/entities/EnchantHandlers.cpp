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

#include "session/handlers/entities/EnchantHandlers.h"

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Log/Log.h"
#include "Object/ObjectMgr.h"
#include "entities/player/Player.h"
#include "Object/Item.h"

/**
 * @brief Wraps an item using wrapping paper.
 *
 * @param recv_data The received opcode packet.
 */
void EnchantHandlers::HandleWrapItem(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;
    // recv_data.hexlike();

    recv_data >> gift_bag >> gift_slot;                     // paper
    recv_data >> item_bag >> item_slot;                     // item

    DEBUG_LOG("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item* gift = session.GetPlayer()->GetInventoryMgr().GetItemByPos(gift_bag, gift_slot);
    if (!gift)
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL);
        return;
    }

    // cheating: non-wrapper wrapper (all empty wrappers are stackable)
    if (!(gift->GetProto()->Flags & ITEM_FLAG_WRAPPER) || gift->GetMaxStackCount() == 1)
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL);
        return;
    }

    Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByPos(item_bag, item_slot);

    if (!item)
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item, NULL);
        return;
    }

    if (item == gift)                                       // not possible with packet from real client
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsEquipped())
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->GetGuidValue(ITEM_FIELD_GIFTCREATOR))         // HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsBag())
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsSoulBound())
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->GetMaxStackCount() != 1)
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    // maybe not correct check  (it is better than nothing)
    if (item->GetProto()->MaxCount > 0)
    {
        session.GetPlayer()->SendEquipError(EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT INTO `character_gifts` VALUES ('%u', '%u', '%u', '%u')", item->GetOwnerGuid().GetCounter(), item->GetGUIDLow(), item->GetEntry(), item->GetUInt32Value(ITEM_FIELD_FLAGS));
    item->SetEntry(gift->GetEntry());

    switch (item->GetEntry())
    {
        case 5042:  item->SetEntry(5043); break;
        case 5048:  item->SetEntry(5044); break;
        case 17303: item->SetEntry(17302); break;
        case 17304: item->SetEntry(17305); break;
        case 17307: item->SetEntry(17308); break;
        case 21830: item->SetEntry(21831); break;
    }
    item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, session.GetPlayer()->GetObjectGuid());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    item->SetState(ITEM_CHANGED, session.GetPlayer());

    if (item->GetState() == ITEM_NEW)                       // save new item, to have alway for `character_gifts` record in `item_instance`
    {
        // after save it will be impossible to remove the item from the queue
        item->RemoveFromUpdateQueueOf(session.GetPlayer());
        item->SaveToDB();                                   // item gave inventory record unchanged and can be save standalone
    }
    CharacterDatabase.CommitTransaction();

    uint32 count = 1;
    session.GetPlayer()->DestroyItemCount(gift, count, true);
}

void EnchantHandlers::HandleSocket(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_SOCKET_GEMS");

    ObjectGuid itemGuid;
    ObjectGuid gemGuids[MAX_GEM_SOCKETS];

    recv_data >> itemGuid;
    if (!itemGuid.IsItem())
    {
        return;
    }

    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        recv_data >> gemGuids[i];
    }

    // cheat -> tried to socket same gem multiple times
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        ObjectGuid gemGuid = gemGuids[i];
        if (!gemGuid)
        {
            continue;
        }

        if (!gemGuid.IsItem())
        {
            return;
        }

        for (int j = i + 1; j < MAX_GEM_SOCKETS; ++j)
            if (gemGuids[j] == gemGuid)
            {
                return;
            }
    }

    Item* itemTarget = session.GetPlayer()->GetInventoryMgr().GetItemByGuid(itemGuid);
    if (!itemTarget)                                        // missing item to socket
    {
        return;
    }

    ItemPrototype const* itemProto = itemTarget->GetProto();
    if (!itemProto)
    {
        return;
    }

    // this slot is excepted when applying / removing meta gem bonus
    uint8 slot = itemTarget->IsEquipped() ? itemTarget->GetSlot() : uint8(NULL_SLOT);

    Item* Gems[MAX_GEM_SOCKETS];
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        Gems[i] = gemGuids[i] ? session.GetPlayer()->GetInventoryMgr().GetItemByGuid(gemGuids[i]) : NULL;
    }

    GemPropertiesEntry const* GemProps[MAX_GEM_SOCKETS];
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)               // get geminfo from dbc storage
    {
        GemProps[i] = (Gems[i]) ? sGemPropertiesStore.LookupEntry(Gems[i]->GetProto()->GemProperties) : NULL;
    }

    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)               // check for hack maybe
    {
        if (!GemProps[i])
        {
            continue;
        }

        // tried to put gem in socket where no socket exists (take care about prismatic sockets)
        if (!itemProto->Socket[i].Color)
        {
            // no prismatic socket
            if (!itemTarget->GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT))
            {
                return;
            }

            // not first not-colored (not normally used) socket
            if (i != 0 && !itemProto->Socket[i - 1].Color && (i + 1 >= MAX_GEM_SOCKETS || itemProto->Socket[i + 1].Color))
            {
                return;
            }

            // ok, this is first not colored socket for item with prismatic socket
        }

        // tried to put normal gem in meta socket
        if (itemProto->Socket[i].Color == SOCKET_COLOR_META && GemProps[i]->Type != SOCKET_COLOR_META)
        {
            return;
        }

        // tried to put meta gem in normal socket
        if (itemProto->Socket[i].Color != SOCKET_COLOR_META && GemProps[i]->Type == SOCKET_COLOR_META)
        {
            return;
        }
    }

    uint32 GemEnchants[MAX_GEM_SOCKETS];
    uint32 OldEnchants[MAX_GEM_SOCKETS];
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)               // get new and old enchantments
    {
        GemEnchants[i] = (GemProps[i]) ? GemProps[i]->Enchant_ID : 0;
        OldEnchants[i] = itemTarget->GetEnchantmentId(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i));
    }

    // check unique-equipped conditions
    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (!Gems[i])
        {
            continue;
        }

        // continue check for case when attempt add 2 similar unique equipped gems in one item.
        ItemPrototype const* iGemProto = Gems[i]->GetProto();

        // unique item (for new and already placed bit removed enchantments
        if (iGemProto->Flags & ITEM_FLAG_UNIQUE_EQUIPPED)
        {
            for (int j = 0; j < MAX_GEM_SOCKETS; ++j)
            {
                if (i == j)                                 // skip self
                {
                    continue;
                }

                if (Gems[j])
                {
                    if (iGemProto->ItemId == Gems[j]->GetEntry())
                    {
                        session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, itemTarget, NULL);
                        return;
                    }
                }
                else if (OldEnchants[j])
                {
                    if (SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(OldEnchants[j]))
                    {
                        if (iGemProto->ItemId == enchantEntry->Src_itemID)
                        {
                            session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, itemTarget, NULL);
                            return;
                        }
                    }
                }
            }
        }

        // unique limit type item
        int32 limit_newcount = 0;
        if (iGemProto->ItemLimitCategory)
        {
            if (ItemLimitCategoryEntry const* limitEntry = sItemLimitCategoryStore.LookupEntry(iGemProto->ItemLimitCategory))
            {
                // NOTE: limitEntry->Flags not checked because if item have have-limit then it applied and to equip case

                for (int j = 0; j < MAX_GEM_SOCKETS; ++j)
                {
                    if (Gems[j])
                    {
                        // destroyed gem
                        if (OldEnchants[j])
                        {
                            if (SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(OldEnchants[j]))
                                if (ItemPrototype const* jProto = ObjectMgr::GetItemPrototype(enchantEntry->Src_itemID))
                                    if (iGemProto->ItemLimitCategory == jProto->ItemLimitCategory)
                                    {
                                        --limit_newcount;
                                    }
                        }

                        // new gem
                        if (iGemProto->ItemLimitCategory == Gems[j]->GetProto()->ItemLimitCategory)
                        {
                            ++limit_newcount;
                        }
                    }
                    // existing gem
                    else if (OldEnchants[j])
                    {
                        if (SpellItemEnchantmentEntry const* enchantEntry = sSpellItemEnchantmentStore.LookupEntry(OldEnchants[j]))
                            if (ItemPrototype const* jProto = ObjectMgr::GetItemPrototype(enchantEntry->Src_itemID))
                                if (iGemProto->ItemLimitCategory == jProto->ItemLimitCategory)
                                {
                                    ++limit_newcount;
                                }
                    }
                }

                if (limit_newcount > 0 && uint32(limit_newcount) > limitEntry->Quantity)
                {
                    session.GetPlayer()->SendEquipError(EQUIP_ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED, itemTarget, NULL);
                    return;
                }
            }
        }

        // for equipped item check all equipment for duplicate equipped gems
        if (itemTarget->IsEquipped())
        {
            if (InventoryResult res = session.GetPlayer()->CanEquipUniqueItem(Gems[i], slot, limit_newcount >= 0 ? limit_newcount : 0))
            {
                session.GetPlayer()->SendEquipError(res, itemTarget, NULL);
                return;
            }
        }
    }

    bool SocketBonusActivated = itemTarget->GemsFitSockets();    // save state of socketbonus
    session.GetPlayer()->ToggleMetaGemsActive(slot, false);             // turn off all metagems (except for the target item)

    // if a meta gem is being equipped, all information has to be written to the item before testing if the conditions for the gem are met

    // remove ALL enchants
    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchant_slot)
    {
        session.GetPlayer()->ApplyEnchantment(itemTarget, EnchantmentSlot(enchant_slot), false);
    }

    for (int i = 0; i < MAX_GEM_SOCKETS; ++i)
    {
        if (GemEnchants[i])
        {
            uint32 count = 1;
            itemTarget->SetEnchantment(EnchantmentSlot(SOCK_ENCHANTMENT_SLOT + i), GemEnchants[i], 0, 0, session.GetPlayer()->GetObjectGuid());
            if (Item* guidItem = gemGuids[i] ? session.GetPlayer()->GetInventoryMgr().GetItemByGuid(gemGuids[i]) : NULL)
            {
                session.GetPlayer()->DestroyItemCount(guidItem, count, true);
            }
        }
    }

    for (uint32 enchant_slot = SOCK_ENCHANTMENT_SLOT; enchant_slot < SOCK_ENCHANTMENT_SLOT + MAX_GEM_SOCKETS; ++enchant_slot)
    {
        session.GetPlayer()->ApplyEnchantment(itemTarget, EnchantmentSlot(enchant_slot), true);
    }

    bool SocketBonusToBeActivated = itemTarget->GemsFitSockets();// current socketbonus state
    if (SocketBonusActivated != SocketBonusToBeActivated)   // if there was a change...
    {
        session.GetPlayer()->ApplyEnchantment(itemTarget, BONUS_ENCHANTMENT_SLOT, false);
        itemTarget->SetEnchantment(BONUS_ENCHANTMENT_SLOT, (SocketBonusToBeActivated ? itemTarget->GetProto()->socketBonus : 0), 0, 0, session.GetPlayer()->GetObjectGuid());
        session.GetPlayer()->ApplyEnchantment(itemTarget, BONUS_ENCHANTMENT_SLOT, true);
        // it is not displayed, client has an inbuilt system to determine if the bonus is activated
    }

    session.GetPlayer()->ToggleMetaGemsActive(slot, true);              // turn on all metagems (except for target item)

    itemTarget->SendUpdateSockets();
}

/**
 * @brief Cancels a temporary weapon enchantment.
 *
 * @param recv_data The received opcode packet.
 */
void EnchantHandlers::HandleCancelTempEnchantment(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_CANCEL_TEMP_ENCHANTMENT");

    uint32 eslot;

    recv_data >> eslot;

    // apply only to equipped item
    if (!InventoryMgr::IsEquipmentPos(INVENTORY_SLOT_BAG_0, eslot))
    {
        return;
    }

    Item* item = session.GetPlayer()->GetInventoryMgr().GetItemByPos(INVENTORY_SLOT_BAG_0, eslot);

    if (!item)
    {
        return;
    }

    if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        return;
    }

    session.GetPlayer()->ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
