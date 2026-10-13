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

#ifndef MANGOS_H_LFGHANDLERS
#define MANGOS_H_LFGHANDLERS

class WorldPacket;
class WorldSession;

/// The client's Dungeon Finder opcodes: joining and leaving the queue, the group's role choice,
/// the status request, the answer to a dungeon proposal, the teleport into and out of the dungeon,
/// the queue comment, the lock information request for the player or the group, and the vote to
/// remove a member: static entry points the opcode table binds, each borrowing the session for one
/// call and storing nothing.
struct LfgHandlers
{
    public:
        static void HandleLfgJoin(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgLeave(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgSetRoles(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgGetStatus(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgProposalResult(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgTeleport(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetLfgComment(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgLockInfoRequest(WorldSession& session, WorldPacket& recv_data);
        static void HandleLfgSetBootVote(WorldSession& session, WorldPacket& recv_data);
};

#endif
