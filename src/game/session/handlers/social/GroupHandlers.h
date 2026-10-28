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

#ifndef MANGOS_H_GROUPHANDLERS
#define MANGOS_H_GROUPHANDLERS

class WorldPacket;
class WorldSession;

/// The client's group opcodes: an invite and its answer, a member removed, the leader set, the
/// group left, the loot method and a loot roll, a minimap ping and a random roll, the raid's target
/// icons and its conversion, roles set and polled, everyone made an assistant, the raid markers
/// cleared, a member's subgroup, assistant flag and assignment, the ready check and its end, a
/// member's stats, the raid lockouts, loot opt-out, the low-level raid setting and the group's
/// join updates. Static entry points the opcode table binds, each borrowing the session for one
/// call and storing nothing.
struct GroupHandlers
{
    public:
        static void HandleGroupInvite(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupInviteResponse(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupUninviteGuid(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupSetLeader(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupDisband(WorldSession& session, WorldPacket& recv_data);
        static void HandleLootMethod(WorldSession& session, WorldPacket& recv_data);
        static void HandleLootRoll(WorldSession& session, WorldPacket& recv_data);
        static void HandleMinimapPing(WorldSession& session, WorldPacket& recv_data);
        static void HandleRandomRoll(WorldSession& session, WorldPacket& recv_data);
        static void HandleRaidTargetUpdate(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupRaidConvert(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetRole(WorldSession& session, WorldPacket& recv_data);
        static void HandleRolePollBegin(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetEveryoneIsAssistant(WorldSession& session, WorldPacket& recv_data);
        static void HandleClearRaidMarker(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupChangeSubGroup(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupAssistantLeader(WorldSession& session, WorldPacket& recv_data);
        static void HandlePartyAssignment(WorldSession& session, WorldPacket& recv_data);
        static void HandleRaidReadyCheck(WorldSession& session, WorldPacket& recv_data);
        static void HandleRaidReadyCheckFinished(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestPartyMemberStats(WorldSession& session, WorldPacket& recv_data);
        static void HandleRequestRaidInfo(WorldSession& session, WorldPacket& recv_data);
        static void HandleOptOutOfLoot(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetAllowLowLevelRaid(WorldSession& session, WorldPacket& recv_data);
        static void HandleGroupRequestJoinUpdates(WorldSession& session, WorldPacket& recv_data);
};

#endif
