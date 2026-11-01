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

#ifndef MANGOS_H_CHANNELNOTICES
#define MANGOS_H_CHANNELNOTICES

#include "Channel.h"

#include <string>
#include <utility>
#include <vector>

class Player;

/**
 * What a chat channel tells its members' clients, one call per notice; who is told is the channel's
 * own send to its members. The session installs the notices that send them at start-up; this base
 * sends nothing.
 */
class ChannelNotices
{
    public:
        virtual ~ChannelNotices();

        /// The member `who` of `channel` is told `notice`.
        virtual void ToOne(Channel& channel, ObjectGuid who, ChannelNotice const& notice);
        /// Every member of `channel`, but for a member who ignores `ignoring` when it is not empty, is
        /// told `notice`.
        virtual void ToAll(Channel& channel, ChannelNotice const& notice, ObjectGuid ignoring);
        /// `player` is told it is not a member of the channel named `channelName`.
        virtual void NotMember(Player& player, std::string const& channelName);
        /// The member `to` of `channel` is told the members it may see, `members`, each a guid and its
        /// flags.
        virtual void MemberList(Channel& channel, ObjectGuid to, std::vector<std::pair<ObjectGuid, uint8> > const& members);
        /// Every member of `channel` is told the member `who` joined or changed, for the client's user list.
        virtual void UserListChanged(Channel& channel, ObjectGuid who);
        /// Every member of `channel` is told the member `who` left, for the client's user list.
        virtual void UserListRemoved(Channel& channel, ObjectGuid who);
        /// Every member of `channel`, but for a member who ignores `ignoring` when it is not empty, is
        /// told `speaker`'s message `text` in `language`.
        virtual void Said(Channel& channel, Player& speaker, char const* text, uint32 language, ObjectGuid ignoring);

        /// The installed notices, or the base when none are installed.
        static ChannelNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static ChannelNotices* Install(ChannelNotices* notices);
};

#define sChannelNotices ChannelNotices::Installed()

#endif
