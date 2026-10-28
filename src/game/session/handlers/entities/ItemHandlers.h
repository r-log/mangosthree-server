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

#ifndef MANGOS_H_ITEMHANDLERS
#define MANGOS_H_ITEMHANDLERS

class WorldPacket;
class WorldSession;

/// The client's inventory opcodes: a stack split, an item swapped inside the bags or between two
/// slots, an item equipped to a slot or to where it fits, an item destroyed, an item read, an item's
/// purchase data for a refund, an item's text and an item reforged. Static entry points the opcode
/// table binds, each borrowing the session for one call and storing nothing.
struct ItemHandlers
{
    public:
        static void HandleSplitItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleSwapInvItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleAutoEquipItemSlot(WorldSession& session, WorldPacket& recv_data);
        static void HandleSwapItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleAutoEquipItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleDestroyItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleReadItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleItemRefundInfoRequest(WorldSession& session, WorldPacket& recv_data);
        static void HandleItemTextQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleReforgeItem(WorldSession& session, WorldPacket& recv_data);
};

#endif
