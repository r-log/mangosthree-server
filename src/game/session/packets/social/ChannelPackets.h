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

#ifndef MANGOS_H_CHANNELPACKETS
#define MANGOS_H_CHANNELPACKETS

#include "social/ChannelNotices.h"

class WorldPacket;

/// Initializes `packet` as the notice `notice` of the channel named `channelName`: the type, the
/// channel's name, then the fields the type carries.
void BuildChannelNotifyPacket(WorldPacket& packet, std::string const& channelName, ChannelNotice const& notice);
/// Initializes `packet` as `channel`'s member list: 1, the name, the flags, the count, then each
/// member's guid and flags.
void BuildChannelListPacket(WorldPacket& packet, Channel const& channel, std::vector<std::pair<ObjectGuid, uint8> > const& members);
/// Initializes `packet` as the user list's added or changed member `who` of `channel`: added for a
/// built-in channel, changed otherwise; the guid, its flags, the channel's flags, the member count
/// and the name.
void BuildUserListChangedPacket(WorldPacket& packet, Channel const& channel, ObjectGuid who);
/// Initializes `packet` as the user list's removed member `who` of `channel`: the guid, the channel's
/// flags, the member count and the name.
void BuildUserListRemovedPacket(WorldPacket& packet, Channel const& channel, ObjectGuid who);

/// The channel's notices, each built here and sent through the channel's own sends to its members.
class SessionChannelNotices : public ChannelNotices
{
    public:
        void ToOne(Channel& channel, ObjectGuid who, ChannelNotice const& notice) override;
        void ToAll(Channel& channel, ChannelNotice const& notice, ObjectGuid ignoring) override;
        void NotMember(Player& player, std::string const& channelName) override;
        void MemberList(Channel& channel, ObjectGuid to, std::vector<std::pair<ObjectGuid, uint8> > const& members) override;
        void UserListChanged(Channel& channel, ObjectGuid who) override;
        void UserListRemoved(Channel& channel, ObjectGuid who) override;
        void Said(Channel& channel, Player& speaker, char const* text, uint32 language, ObjectGuid ignoring) override;
};

#endif
