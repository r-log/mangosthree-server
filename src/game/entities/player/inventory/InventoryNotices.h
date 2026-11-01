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

#ifndef MANGOS_H_INVENTORYNOTICES
#define MANGOS_H_INVENTORYNOTICES

#include "Item.h"
#include "Player.h"
#include "SharedDefines.h"

class Group;

/**
 * What a player's bags, loot, mail and purchases tell its own client, one call per notice, and an
 * item it receives to its group. The player keeps when each is told. The session installs the
 * notices that send them at start-up; this base sends nothing.
 */
class InventoryNotices
{
    public:
        virtual ~InventoryNotices();

        /// `player` is told its equipment sets `sets`, the deleted ones left out.
        virtual void EquipmentSetsShown(Player const& player, EquipmentSets const& sets);
        /// `player` is told its equipment set `index` was saved as `setGuid`.
        virtual void EquipmentSetSaved(Player const& player, uint32 index, uint64 setGuid);
        /// `player` is told it got `count` of `item` (`received` from an npc, `created`), with `inventoryCount`
        /// of them now in its bags; `group`, when given, is told instead.
        virtual void ItemPushed(Player const& player, Item const& item, uint32 count, bool received, bool created, uint32 inventoryCount, Group* group);
        /// `player` is told the inventory change `result` of `item` and `item2` failed (the item `itemId` when
        /// neither names it).
        virtual void EquipFailed(Player const& player, InventoryResult result, Item const* item, Item const* item2, uint32 itemId);
        /// `player` is told buying `item` from `vendor` failed with `result` and `param`.
        virtual void BuyFailed(Player const& player, BuyResult result, ObjectGuid vendor, uint32 item, uint32 param);
        /// `player` is told selling `item` to `vendor` failed with `result` and `param`.
        virtual void SellFailed(Player const& player, SellResult result, ObjectGuid vendor, ObjectGuid item, uint32 param);
        /// `player` is told it released the loot of `loot`.
        virtual void LootReleased(Player const& player, ObjectGuid loot);
        /// `player` is shown the loot `contents` of `loot` as `type`, as its `permission` lets it see them.
        virtual void LootShown(Player const& player, ObjectGuid loot, LootType type, Loot& contents, PermissionTypes permission);
        /// `player` is told the money left the loot it views.
        virtual void LootMoneyCleared(Player const& player);
        /// `player` is told the loot slot `slot` (a currency's when `currency`) is empty.
        virtual void LootSlotRemoved(Player const& player, uint8 slot, bool currency);
        /// `player` is told the result `error` of the mail action `action` on the mail `mailId`.
        virtual void MailResult(Player const& player, uint32 mailId, MailResponseType action, MailResponseResult error, uint32 equipError, uint32 itemGuid, uint32 itemCount);
        /// `player` is told new mail arrived.
        virtual void MailReceived(Player const& player);
        /// `player` is told it bought `count` from `vendor`'s slot `vendorSlot`, which has `remaining` left
        /// (0xFFFFFFFF for no limit).
        virtual void ItemBought(Player const& player, ObjectGuid vendor, uint32 vendorSlot, uint32 remaining, uint32 count);

        /// The installed notices, or the base when none are installed.
        static InventoryNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static InventoryNotices* Install(InventoryNotices* notices);
};

#define sInventoryNotices InventoryNotices::Installed()

#endif
