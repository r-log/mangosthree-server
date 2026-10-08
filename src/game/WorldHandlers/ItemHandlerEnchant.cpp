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

#include "Platform/Define.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"

/**
 * @file ItemHandlerEnchant.cpp
 * @brief Cohesion split of ItemHandler.cpp -- enchantment, gem-socket and wrap opcode handlers: enchant log / time-update packets, wrap item, socket gems and cancel temporary enchantment. Same WorldSession class; no behaviour change. CMake file(GLOB) picks this file up automatically; WorldSession.h is unchanged.
 */

/**
 * @brief Sends an enchantment log packet to the client.
 *
 * @param targetGuid The enchanted target guid.
 * @param casterGuid The caster guid.
 * @param itemId The item entry id.
 * @param enchantId The enchantment spell id.
 */
void WorldSession::SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 enchantId)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8 + 8 + 4 + 4 + 1)); // last check 2.0.10
    data << targetGuid.WriteAsPacked();
    data << casterGuid.WriteAsPacked();
    data << uint32(itemId);
    data << uint32(enchantId);
    SendPacket(&data);
}

/**
 * @brief Sends a temporary enchantment timer update.
 *
 * @param playerGuid The owning player guid.
 * @param itemGuid The enchanted item guid.
 * @param slot The equipment slot index.
 * @param duration The remaining duration in milliseconds.
 */
void WorldSession::SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration)
{
    // last check 2.0.10
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8 + 4 + 4 + 8));
    data << ObjectGuid(itemGuid);
    data << uint32(slot);
    data << uint32(duration);
    data << ObjectGuid(playerGuid);
    SendPacket(&data);
}
