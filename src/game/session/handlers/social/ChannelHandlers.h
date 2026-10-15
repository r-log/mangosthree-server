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

#ifndef MANGOS_H_CHANNELHANDLERS
#define MANGOS_H_CHANNELHANDLERS

class WorldPacket;
class WorldSession;

/// Handles the client's chat channel requests: joining and leaving a channel, its member list,
/// password and owner, moderators, mutes, invitations, kicks, bans and join announcements.
struct ChannelHandlers
{
    public:
        static void HandleJoinChannel(WorldSession& session, WorldPacket& recvPacket);
        static void HandleLeaveChannel(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelList(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelPassword(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelSetOwner(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelOwner(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelModerator(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelUnmoderator(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelMute(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelUnmute(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelInvite(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelKick(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelBan(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelUnban(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelAnnouncements(WorldSession& session, WorldPacket& recvPacket);
        static void HandleChannelDisplayListQuery(WorldSession& session, WorldPacket& recvPacket);
        static void HandleSetChannelWatch(WorldSession& session, WorldPacket& recvPacket);
};

#endif
