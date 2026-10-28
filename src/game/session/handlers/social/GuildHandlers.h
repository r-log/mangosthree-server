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

#ifndef MANGOS_H_GUILDHANDLERS
#define MANGOS_H_GUILDHANDLERS

class WorldPacket;
class WorldSession;

/// The client's guild opcodes: the guild's query, roster, ranks and event log, an invite and its
/// answer, a member removed, promoted, demoted, given a rank or a note, the guild left, disbanded
/// or handed to a new leader, its message of the day, ranks and information text edited, its
/// emblem saved and its auto-decline toggled, and the guild bank: its banker, tabs, money, items,
/// logs and tab texts. Static entry points the opcode table binds, each borrowing the session for
/// one call and storing nothing.
struct GuildHandlers
{
    public:
        static void HandleGuildQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildInvite(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildRemove(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildAccept(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildDecline(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildRoster(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildPromote(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildDemote(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildSetRank(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildSwitchRank(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildLeave(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildDisband(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildLeader(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildMOTD(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildSetNote(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildRank(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildAddRank(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildDelRank(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildChangeInfoText(WorldSession& session, WorldPacket& recv_data);
        static void HandleSaveGuildEmblem(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildEventLogQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankMoneyWithdrawn(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildPermissions(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankerActivate(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankQueryTab(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankDepositMoney(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankWithdrawMoney(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankSwapItems(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankBuyTab(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankUpdateTab(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildBankLogQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleQueryGuildBankTabText(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetGuildBankTabText(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildQueryRanks(WorldSession& session, WorldPacket& recv_data);
        static void HandleGuildAutoDeclineToggle(WorldSession& session, WorldPacket& recv_data);
};

#endif
