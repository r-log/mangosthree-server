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

#ifndef MANGOS_H_PLAYERHANDLERS
#define MANGOS_H_PLAYERHANDLERS

class WorldPacket;
class WorldSession;

/// The client's opcodes about its own player and the players it inspects: the selection, the stand
/// state, an action button and the action bars shown, the title, the mount aura cancelled, the far
/// sight view, the played time, the raid-frame profiles saved, a cinematic's camera and its end,
/// and the inspect of another player's gear, honor and achievements. Static entry points the
/// opcode table binds, each borrowing the session for one call and storing nothing.
struct PlayerHandlers
{
    public:
        static void HandleSetSelection(WorldSession& session, WorldPacket& recv_data);
        static void HandleStandStateChange(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetActionButton(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetActionBarToggles(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetTitle(WorldSession& session, WorldPacket& recv_data);
        static void HandleCancelMountAura(WorldSession& session, WorldPacket& recv_data);
        static void HandleFarSight(WorldSession& session, WorldPacket& recv_data);
        static void HandlePlayedTime(WorldSession& session, WorldPacket& recv_data);
        static void HandleSaveCUFProfiles(WorldSession& session, WorldPacket& recv_data);
        static void HandleCompleteCinematic(WorldSession& session, WorldPacket& recv_data);
        static void HandleNextCinematicCamera(WorldSession& session, WorldPacket& recv_data);
        static void HandleInspect(WorldSession& session, WorldPacket& recv_data);
        static void HandleInspectHonorStats(WorldSession& session, WorldPacket& recv_data);
        static void HandleQueryInspectAchievements(WorldSession& session, WorldPacket& recv_data);
};

#endif
