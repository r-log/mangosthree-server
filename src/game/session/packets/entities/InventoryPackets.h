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

#ifndef MANGOS_H_INVENTORYPACKETS
#define MANGOS_H_INVENTORYPACKETS

#include "entities/player/inventory/InventoryNotices.h"

/// A player's inventory, loot, mail and purchase notices, each built here and sent to the player's own
/// client, an item received to its group when it names one.
class SessionInventoryNotices : public InventoryNotices
{
    public:
        void EquipmentSetsShown(Player const& player, EquipmentSets const& sets) override;
        void EquipmentSetSaved(Player const& player, uint32 index, uint64 setGuid) override;
        void ItemPushed(Player const& player, Item const& item, uint32 count, bool received, bool created, uint32 inventoryCount, Group* group) override;
        void EquipFailed(Player const& player, InventoryResult result, Item const* item, Item const* item2, uint32 itemId) override;
        void BuyFailed(Player const& player, BuyResult result, ObjectGuid vendor, uint32 item, uint32 param) override;
        void SellFailed(Player const& player, SellResult result, ObjectGuid vendor, ObjectGuid item, uint32 param) override;
        void LootReleased(Player const& player, ObjectGuid loot) override;
        void LootShown(Player const& player, ObjectGuid loot, LootType type, Loot& contents, PermissionTypes permission) override;
        void LootMoneyCleared(Player const& player) override;
        void LootSlotRemoved(Player const& player, uint8 slot, bool currency) override;
        void MailResult(Player const& player, uint32 mailId, MailResponseType action, MailResponseResult error, uint32 equipError, uint32 itemGuid, uint32 itemCount) override;
        void MailReceived(Player const& player) override;
        void ItemBought(Player const& player, ObjectGuid vendor, uint32 vendorSlot, uint32 remaining, uint32 count) override;
};

#endif
