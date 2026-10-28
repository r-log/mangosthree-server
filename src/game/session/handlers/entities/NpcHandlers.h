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

#ifndef MANGOS_H_NPCHANDLERS
#define MANGOS_H_NPCHANDLERS

class ObjectGuid;
class WorldPacket;
class WorldSession;

/// The client's NPC service opcodes: the tabard vendor, the banker, a trainer's list and a spell
/// bought from it, gossip opened and an option chosen, the spirit healer, the innkeeper's binding,
/// the stable's list, a pet moved between its slots or out of the stable, and repairs. Static entry
/// points the opcode table binds, each borrowing the session for one call and storing nothing.
struct NpcHandlers
{
    public:
        static void HandleTabardVendorActivate(WorldSession& session, WorldPacket& recv_data);
        static void HandleBankerActivate(WorldSession& session, WorldPacket& recv_data);
        static void HandleTrainerList(WorldSession& session, WorldPacket& recv_data);
        static void HandleTrainerBuySpell(WorldSession& session, WorldPacket& recv_data);
        static void HandleGossipHello(WorldSession& session, WorldPacket& recv_data);
        static void HandleGossipSelectOption(WorldSession& session, WorldPacket& recv_data);
        static void HandleSpiritHealerActivate(WorldSession& session, WorldPacket& recv_data);
        static void HandleBinderActivate(WorldSession& session, WorldPacket& recv_data);
        static void HandleListStabledPets(WorldSession& session, WorldPacket& recv_data);
        static void HandleStablePet(WorldSession& session, WorldPacket& recv_data);
        static void HandleUnstablePet(WorldSession& session, WorldPacket& recv_data);
        static void HandleRepairItem(WorldSession& session, WorldPacket& recv_data);

    private:
        static bool CheckStableMaster(WorldSession& session, ObjectGuid guid);
};

#endif
