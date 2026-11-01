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

#ifndef MANGOS_H_GUILDNOTICES
#define MANGOS_H_GUILDNOTICES

#include "Guild.h"

#include <string>

class Player;

/**
 * What a guild tells its members' clients, one call per notice; who is told is the guild's own send
 * to its members. The session installs the notices that send them at start-up; this base sends
 * nothing.
 */
class GuildNotices
{
    public:
        virtual ~GuildNotices();

        /// The members of `guild` are told the event `event`, about the player `guid` (an empty guid
        /// names none) and with up to three strings, `str1` first.
        virtual void Event(Guild& guild, GuildEvents event, ObjectGuid guid, char const* str1, char const* str2, char const* str3);
        /// The members of `guild` who may listen are told `speaker`'s message `msg` in `language`, in
        /// the officer chat when `officers` is true and the guild chat otherwise.
        virtual void Chat(Guild& guild, Player& speaker, std::string const& msg, uint32 language, bool officers);
        /// The members of `guild` who may listen are told `speaker`'s addon message `msg` with the prefix
        /// `prefix`, in the officer chat when `officers` is true and the guild chat otherwise.
        virtual void AddonChat(Guild& guild, Player& speaker, std::string const& msg, std::string const& prefix, bool officers);

        /// The installed notices, or the base when none are installed.
        static GuildNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static GuildNotices* Install(GuildNotices* notices);
};

#define sGuildNotices GuildNotices::Installed()

#endif
