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

#ifndef MANGOS_H_PVPHANDLERS
#define MANGOS_H_PVPHANDLERS

class WorldPacket;
class WorldSession;

/// The client's PvP opcodes: the arena team's inspect, query, roster, create, invite, accept,
/// decline, leave, disband, remove and leader opcodes; the battlemaster's hello and join, the
/// arena join, the battlefield list, port, leave and status, the flag carriers' positions, the
/// PvP log, the spirit healer's query and queue, the AFK report, and the rated battleground
/// stats, rated battleground info, PvP options and PvP rewards requests: static entry points the
/// opcode table binds, each borrowing the session for one call and storing nothing.
struct PvpHandlers
{
    public:
        static void HandleInspectArenaTeams(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamRoster(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamCreate(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamInvite(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamAccept(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamDecline(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamLeave(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamDisband(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamRemove(WorldSession& session, WorldPacket& recv_data);
        static void HandleArenaTeamLeader(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattlemasterHello(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattlemasterJoin(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattleGroundPlayerPositions(WorldSession& session, WorldPacket& recv_data);
        static void HandlePVPLogData(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattlefieldList(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattleFieldPort(WorldSession& session, WorldPacket& recv_data);
        static void HandleLeaveBattlefield(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattlefieldStatus(WorldSession& session, WorldPacket& recv_data);
        static void HandleAreaSpiritHealerQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleAreaSpiritHealerQueue(WorldSession& session, WorldPacket& recv_data);
        static void HandleBattlemasterJoinArena(WorldSession& session, WorldPacket& recv_data);
        static void HandleReportPvPAFK(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestRatedBGStats(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestPvPOptionsEnabled(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestPvPRewards(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestRatedBgInfo(WorldSession& session, WorldPacket& recv_data);
};

#endif
