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

#include "InventoryPackets.h"
#include "Group.h"
#include "Item.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void SessionInventoryNotices::EquipmentSetsShown(Player const& player, EquipmentSets const& sets)
{
    uint32 count = 0;
    WorldPacket data(SMSG_LOAD_EQUIPMENT_SET, 4);
    size_t count_pos = data.wpos();
    data << uint32(count);                                  // count placeholder
    for (EquipmentSets::const_iterator itr = sets.begin(); itr != sets.end(); ++itr)
    {
        if (itr->second.state == EQUIPMENT_SET_DELETED)
        {
            continue;
        }
        data.appendPackGUID(itr->second.Guid);
        data << uint32(itr->first);
        data << itr->second.Name;
        data << itr->second.IconName;
        for (uint32 i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            // ignored slots stored in IgnoreMask, client wants "1" as raw GUID, so no HIGHGUID_ITEM
            if (itr->second.IgnoreMask & (1 << i))
            {
                data << ObjectGuid(uint64(1)).WriteAsPacked();
            }
            else
            {
                data << ObjectGuid(HIGHGUID_ITEM, itr->second.Items[i]).WriteAsPacked();
            }
        }

        ++count;                                            // client have limit but it checked at loading and set
    }
    data.put<uint32>(count_pos, count);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::EquipmentSetSaved(Player const& player, uint32 index, uint64 setGuid)
{
    WorldPacket data(SMSG_EQUIPMENT_SET_ID, 4 + 1);
    data << uint32(index);
    data.appendPackGUID(setGuid);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::ItemPushed(Player const& player, Item const& item, uint32 count, bool received, bool created, uint32 inventoryCount, Group* group)
{
    // last check 2.0.10
    WorldPacket data(SMSG_ITEM_PUSH_RESULT, (8 + 4 + 4 + 4 + 1 + 4 + 4 + 4 + 4 + 4));
    data << player.GetObjectGuid();                                // player GUID
    data << uint32(received);                               // 0=looted, 1=from npc
    data << uint32(created);                                // 0=received, 1=created
    data << uint32(1);                                      // IsShowChatMessage
    data << uint8(item.GetBagSlot());                      // bagslot
    // item slot, but when added to stack: 0xFFFFFFFF
    data << uint32((item.GetCount() == count) ? item.GetSlot() : -1);
    data << uint32(item.GetEntry());                       // item id
    data << uint32(item.GetItemSuffixFactor());            // SuffixFactor
    data << uint32(item.GetItemRandomPropertyId());        // random item property id
    data << uint32(count);                                  // count of items
    data << uint32(inventoryCount);         // count of items in inventory

    if (group)
    {
        group->BroadcastPacket(&data, true);
    }
    else
    {
        player.SendDirectMessage(&data);
    }
}

void SessionInventoryNotices::EquipFailed(Player const& player, InventoryResult result, Item const* item, Item const* item2, uint32 itemId)
{
    WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE, 1 + 8 + 8 + 1);
    data << uint8(result);

    if (result != EQUIP_ERR_OK)
    {
        data << (item ? item->GetObjectGuid() : ObjectGuid());
        data << (item2 ? item2->GetObjectGuid() : ObjectGuid());
        data << uint8(0);                                   // bag type subclass, used with EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM and EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2

        switch (result)
        {
            case EQUIP_ERR_CANT_EQUIP_LEVEL_I:
            case EQUIP_ERR_PURCHASE_LEVEL_TOO_LOW:
            {
                ItemPrototype const* proto = item ? item->GetProto() : ObjectMgr::GetItemPrototype(itemId);
                data << uint32(proto ? proto->RequiredLevel : 0);
                break;
            }
            case EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM:    // no idea about this one...
            {
                data << uint64(0);
                data << uint32(0);
                data << uint64(0);
                break;
            }
            case EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS:
            case EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_SOCKETED_EXCEEDED_IS:
            case EQUIP_ERR_ITEM_MAX_LIMIT_CATEGORY_EQUIPPED_EXCEEDED_IS:
            {
                ItemPrototype const* proto = item ? item->GetProto() : ObjectMgr::GetItemPrototype(itemId);
                data << uint32(proto ? proto->ItemLimitCategory : 0);
                break;
            }
            default:
                break;
        }
    }
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::BuyFailed(Player const& player, BuyResult result, ObjectGuid vendor, uint32 item, uint32 param)
{
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 4 + 1));
    data << vendor;
    data << uint32(item);
    if (param > 0)
    {
        data << uint32(param);
    }
    data << uint8(result);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::SellFailed(Player const& player, SellResult result, ObjectGuid vendor, ObjectGuid item, uint32 param)
{
    WorldPacket data(SMSG_SELL_ITEM, (8 + 8 + (param ? 4 : 0) + 1)); // last check 2.0.10
    data << vendor;
    data << ObjectGuid(item);
    if (param > 0)
    {
        data << uint32(param);
    }
    data << uint8(result);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::LootReleased(Player const& player, ObjectGuid loot)
{
    WorldPacket data(SMSG_LOOT_RELEASE_RESPONSE, (8 + 1));
    data << loot;
    data << uint8(1);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::LootShown(Player const& player, ObjectGuid loot, LootType type, Loot& contents, PermissionTypes permission)
{
    WorldPacket data(SMSG_LOOT_RESPONSE, (9 + 50));         // we guess size
    data << ObjectGuid(loot);
    data << uint8(type);
    data << LootView(contents, &player, permission);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::LootMoneyCleared(Player const& player)
{
    WorldPacket data(SMSG_LOOT_CLEAR_MONEY, 0);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::LootSlotRemoved(Player const& player, uint8 slot, bool currency)
{
    WorldPacket data(currency ? SMSG_LOOT_CURRENCY_REMOVED : SMSG_LOOT_REMOVED, 1);
    data << uint8(slot);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::MailResult(Player const& player, uint32 mailId, MailResponseType action, MailResponseResult error, uint32 equipError, uint32 itemGuid, uint32 itemCount)
{
    WorldPacket data(SMSG_SEND_MAIL_RESULT, (4 + 4 + 4 + (error == MAIL_ERR_EQUIP_ERROR ? 4 : (action == MAIL_ITEM_TAKEN ? 4 + 4 : 0))));
    data << (uint32) mailId;
    data << (uint32) action;
    data << (uint32) error;
    if (error == MAIL_ERR_EQUIP_ERROR)
    {
        data << (uint32) equipError;
    }
    else if (action == MAIL_ITEM_TAKEN)
    {
        data << (uint32) itemGuid;                         // item guid low?
        data << (uint32) itemCount;                        // item count?
    }
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::MailReceived(Player const& player)
{
    // deliver undelivered mail
    WorldPacket data(SMSG_RECEIVED_MAIL, 4);
    data << float(0.0f);
    player.SendDirectMessage(&data);
}

void SessionInventoryNotices::ItemBought(Player const& player, ObjectGuid vendor, uint32 vendorSlot, uint32 remaining, uint32 count)
{
    WorldPacket data(SMSG_BUY_ITEM, 8 + 4 + 4 + 4);
    data << vendor;
    data << uint32(vendorSlot + 1);                 // numbered from 1 at client
    data << uint32(remaining);
    data << uint32(count);
    player.SendDirectMessage(&data);
}
