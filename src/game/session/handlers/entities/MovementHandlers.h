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

#ifndef MANGOS_H_MOVEMENTHANDLERS
#define MANGOS_H_MOVEMENTHANDLERS

class MovementInfo;
class Unit;
class WorldPacket;
class WorldSession;

/// The client's movement opcodes: the far and near teleport acks, the movement words, the
/// movement acks, the mover selected or let go, the time skipped, the mount's special animation
/// and the answer to a summon. Static entry points the opcode table binds, each borrowing the
/// session for one call and storing nothing.
struct MovementHandlers
{
    public:
        static void HandleMoveWorldportAck(WorldSession& session, WorldPacket& recv_data);
        static void HandleMoveTeleportAck(WorldSession& session, WorldPacket& recv_data);
        static void HandleMovement(WorldSession& session, WorldPacket& recv_data);
        /// Every movement ack the registry has a layout for: decoded,
        /// matched against the mover's pending change, relocated on a match, and answered
        /// with the observer packet the matrix names. The teleport ack has its own.
        static void HandleMovementAck(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetActiveMover(WorldSession& session, WorldPacket& recv_data);
        static void HandleMoveNotActiveMover(WorldSession& session, WorldPacket& recv_data);
        static void HandleMoveTimeSkipped(WorldSession& session, WorldPacket& recv_data);
        static void HandleMountSpecialAnim(WorldSession& session, WorldPacket& recvdata);
        static void HandleSummonResponse(WorldSession& session, WorldPacket& recv_data);

        /// A validated movement word applied to the mover: the store, the transport handling,
        /// the relocation and the edges read off the word (the mount's lift-off). Public only
        /// so the headless harness can drive the real thing rather than a copy of it -- the
        /// mount-pet rule is decided in here and nowhere else, and a scenario re-implementing
        /// the edge would prove its own copy and not the handler. Every real caller is one of
        /// the movement opcode handlers here.
        static void HandleMoverRelocation(WorldSession& session, Unit* mover, MovementInfo& movementInfo);

    private:
        static bool VerifyMovementInfo(WorldSession const& session, MovementInfo const& movementInfo);
};

#endif
