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

#include "entities/player/inventory/InventoryNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<InventoryNotices*> s_inventoryNoticesInstalled(NULL);

    InventoryNotices& BaseInventoryNotices()
    {
        static InventoryNotices notices;
        return notices;
    }
}

InventoryNotices::~InventoryNotices()
{
}

void InventoryNotices::EquipmentSetsShown(Player const& /*player*/, EquipmentSets const& /*sets*/)
{
}

void InventoryNotices::EquipmentSetSaved(Player const& /*player*/, uint32 /*index*/, uint64 /*setGuid*/)
{
}

void InventoryNotices::ItemPushed(Player const& /*player*/, Item const& /*item*/, uint32 /*count*/, bool /*received*/, bool /*created*/, uint32 /*inventoryCount*/, Group* /*group*/)
{
}

void InventoryNotices::EquipFailed(Player const& /*player*/, InventoryResult /*result*/, Item const* /*item*/, Item const* /*item2*/, uint32 /*itemId*/)
{
}

void InventoryNotices::BuyFailed(Player const& /*player*/, BuyResult /*result*/, ObjectGuid /*vendor*/, uint32 /*item*/, uint32 /*param*/)
{
}

void InventoryNotices::SellFailed(Player const& /*player*/, SellResult /*result*/, ObjectGuid /*vendor*/, ObjectGuid /*item*/, uint32 /*param*/)
{
}

void InventoryNotices::LootReleased(Player const& /*player*/, ObjectGuid /*loot*/)
{
}

void InventoryNotices::LootShown(Player const& /*player*/, ObjectGuid /*loot*/, LootType /*type*/, Loot& /*contents*/, PermissionTypes /*permission*/)
{
}

void InventoryNotices::LootMoneyCleared(Player const& /*player*/)
{
}

void InventoryNotices::LootSlotRemoved(Player const& /*player*/, uint8 /*slot*/, bool /*currency*/)
{
}

void InventoryNotices::MailResult(Player const& /*player*/, uint32 /*mailId*/, MailResponseType /*action*/, MailResponseResult /*error*/, uint32 /*equipError*/, uint32 /*itemGuid*/, uint32 /*itemCount*/)
{
}

void InventoryNotices::MailReceived(Player const& /*player*/)
{
}

void InventoryNotices::ItemBought(Player const& /*player*/, ObjectGuid /*vendor*/, uint32 /*vendorSlot*/, uint32 /*remaining*/, uint32 /*count*/)
{
}

InventoryNotices& InventoryNotices::Installed()
{
    InventoryNotices* notices = s_inventoryNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseInventoryNotices();
}

InventoryNotices* InventoryNotices::Install(InventoryNotices* notices)
{
    return s_inventoryNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
