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

#ifndef MANGOS_H_VENDORHANDLERS
#define MANGOS_H_VENDORHANDLERS

class WorldPacket;
class WorldSession;

/// The client's vendor and bank opcodes: selling, buying back, buying and listing a vendor's
/// items, storing an item into a bag, buying a bank bag slot and moving an item into or out of
/// the bank: static entry points the opcode table binds, each borrowing the session for one call
/// and storing nothing.
struct VendorHandlers
{
    public:
        static void HandleSellItemOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleBuybackItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleBuyItemOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleListInventoryOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleAutoStoreBagItemOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleBuyBankSlotOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleAutoBankItemOpcode(WorldSession& session, WorldPacket& recv_data);
        static void HandleAutoStoreBankItemOpcode(WorldSession& session, WorldPacket& recv_data);
};

#endif
