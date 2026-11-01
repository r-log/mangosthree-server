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

#include "ChannelPackets.h"
#include "Opcodes.h"
#include "Player.h"
#include "WorldPacket.h"
#include "session/packets/chat/MessageChatPacket.h"

void BuildChannelNotifyPacket(WorldPacket& packet, std::string const& channelName, ChannelNotice const& notice)
{
    packet.Initialize(SMSG_CHANNEL_NOTIFY, 1 + channelName.size() + 1);
    packet << uint8(notice.type);
    packet << channelName;

    switch (notice.type)
    {
        case CHAT_JOINED_NOTICE:
        case CHAT_LEFT_NOTICE:
        case CHAT_PASSWORD_CHANGED_NOTICE:
        case CHAT_OWNER_CHANGED_NOTICE:
        case CHAT_ANNOUNCEMENTS_ON_NOTICE:
        case CHAT_ANNOUNCEMENTS_OFF_NOTICE:
        case CHAT_MODERATION_ON_NOTICE:
        case CHAT_MODERATION_OFF_NOTICE:
        case CHAT_PLAYER_ALREADY_MEMBER_NOTICE:
        case CHAT_INVITE_NOTICE:
        case CHAT_VOICE_ON_NOTICE:
        case CHAT_VOICE_OFF_NOTICE:
            packet << ObjectGuid(notice.guid);
            break;
        case CHAT_YOU_JOINED_NOTICE:
            packet << uint8(notice.flags);
            packet << uint32(notice.channelId);
            packet << uint32(0);
            break;
        case CHAT_YOU_LEFT_NOTICE:
            packet << uint32(notice.channelId);
            packet << uint8(0);                             // can be 0x00 and 0x01
            break;
        case CHAT_PLAYER_NOT_FOUND_NOTICE:
        case CHAT_CHANNEL_OWNER_NOTICE:
        case CHAT_PLAYER_NOT_BANNED_NOTICE:
        case CHAT_PLAYER_INVITED_NOTICE:
        case CHAT_PLAYER_INVITE_BANNED_NOTICE:
            packet << notice.name;
            break;
        case CHAT_MODE_CHANGE_NOTICE:
            packet << ObjectGuid(notice.guid);
            packet << uint8(notice.oldFlags);
            packet << uint8(notice.flags);
            break;
        case CHAT_PLAYER_KICKED_NOTICE:
        case CHAT_PLAYER_BANNED_NOTICE:
        case CHAT_PLAYER_UNBANNED_NOTICE:
            packet << ObjectGuid(notice.guid);
            packet << ObjectGuid(notice.source);
            break;
        default:
            break;
    }
}

void BuildChannelListPacket(WorldPacket& packet, Channel const& channel, std::vector<std::pair<ObjectGuid, uint8> > const& members)
{
    packet.Initialize(SMSG_CHANNEL_LIST, 1 + (channel.GetName().size() + 1) + 1 + 4 + channel.GetNumPlayers() * (8 + 1));
    packet << uint8(1);                                     // channel type?
    packet << channel.GetName();                            // channel name
    packet << uint8(channel.GetFlags());                    // channel flags?
    packet << uint32(members.size());
    for (std::vector<std::pair<ObjectGuid, uint8> >::const_iterator itr = members.begin(); itr != members.end(); ++itr)
    {
        packet << ObjectGuid(itr->first);
        packet << uint8(itr->second);                       // flags seems to be changed...
    }
}

void BuildUserListChangedPacket(WorldPacket& packet, Channel const& channel, ObjectGuid who)
{
    if (channel.IsConstant())
    {
        packet.Initialize(SMSG_USERLIST_ADD, 8 + 1 + 1 + 4 + channel.GetName().size() + 1);
    }
    else
    {
        packet.Initialize(SMSG_USERLIST_UPDATE, 8 + 1 + 1 + 4 + channel.GetName().size() + 1);
    }

    packet << ObjectGuid(who);
    packet << uint8(channel.GetPlayerFlags(who));
    packet << uint8(channel.GetFlags());
    packet << uint32(channel.GetNumPlayers());
    packet << channel.GetName();
}

void BuildUserListRemovedPacket(WorldPacket& packet, Channel const& channel, ObjectGuid who)
{
    packet.Initialize(SMSG_USERLIST_REMOVE, 8 + 1 + 4 + channel.GetName().size() + 1);
    packet << ObjectGuid(who);
    packet << uint8(channel.GetFlags());
    packet << uint32(channel.GetNumPlayers());
    packet << channel.GetName();
}

void SessionChannelNotices::ToOne(Channel& channel, ObjectGuid who, ChannelNotice const& notice)
{
    WorldPacket data;
    BuildChannelNotifyPacket(data, channel.GetName(), notice);
    channel.SendToOne(&data, who);
}

void SessionChannelNotices::ToAll(Channel& channel, ChannelNotice const& notice, ObjectGuid ignoring)
{
    WorldPacket data;
    BuildChannelNotifyPacket(data, channel.GetName(), notice);
    channel.SendToAll(&data, ignoring);
}

void SessionChannelNotices::NotMember(Player& player, std::string const& channelName)
{
    ChannelNotice notice;
    notice.type = CHAT_NOT_MEMBER_NOTICE;
    WorldPacket data;
    BuildChannelNotifyPacket(data, channelName, notice);
    player.GetSession()->SendPacket(&data);
}

void SessionChannelNotices::MemberList(Channel& channel, ObjectGuid to, std::vector<std::pair<ObjectGuid, uint8> > const& members)
{
    WorldPacket data;
    BuildChannelListPacket(data, channel, members);
    channel.SendToOne(&data, to);
}

void SessionChannelNotices::UserListChanged(Channel& channel, ObjectGuid who)
{
    WorldPacket data;
    BuildUserListChangedPacket(data, channel, who);
    channel.SendToAll(&data);
}

void SessionChannelNotices::UserListRemoved(Channel& channel, ObjectGuid who)
{
    WorldPacket data;
    BuildUserListRemovedPacket(data, channel, who);
    channel.SendToAll(&data);
}

void SessionChannelNotices::Said(Channel& channel, Player& speaker, char const* text, uint32 language, ObjectGuid ignoring)
{
    WorldPacket data;
    BuildMessageChatPacket(data, CHAT_MSG_CHANNEL, text, Language(language), speaker.GetChatTag(), speaker.GetObjectGuid(), speaker.GetName(), ObjectGuid(), "", channel.GetName().c_str());
    channel.SendToAll(&data, ignoring);
}
