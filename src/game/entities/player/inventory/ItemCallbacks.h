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

#ifndef MANGOS_H_ITEMCALLBACKS
#define MANGOS_H_ITEMCALLBACKS

#include "InventoryMgr.h"                                   // EquipmentSlots
#include "ObjectGuid.h"
#include "SharedDefines.h"

#include <functional>

class Item;
class Unit;

/// What Unit asks of and tells a player's inventory, which the player installs for itself: the item
/// an aura was cast from, found by guid; the durability point a hit done costs an equipped item; and
/// the combat spells of the weapon a hit was made with.
struct ItemCallbacks
{
    std::function<Item*(ObjectGuid guid)> findItem;
    std::function<void(EquipmentSlots slot)> durabilityPointLoss;
    std::function<void(Unit& target, WeaponAttackType attackType)> castItemCombatSpells;
};

#endif
