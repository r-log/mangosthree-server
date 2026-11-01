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

#ifndef MANGOS_H_CHATNOTICES
#define MANGOS_H_CHATNOTICES

#include "Platform/Define.h"
#include "SharedDefines.h"

#include <string>
#include <vector>

class Player;
class Unit;
class WorldObject;
struct MangosStringLocale;

/**
 * What a speaker's chat tells the clients that hear it, one call per notice: a creature's say, yell,
 * emote and localized text, a player's say, yell, emote and whisper. The speaker keeps who hears it:
 * the range, the team and the listeners. The session installs the notices that send them at
 * start-up; this base sends nothing.
 */
class ChatNotices
{
    public:
        virtual ~ChatNotices();

        /// The players within `range` of `speaker`, the speaker among them, hear it say `text` as
        /// `msgtype`, naming `target`.
        virtual void MonsterSaid(WorldObject const& speaker, ChatMsg msgtype, char const* text, Unit const* target, float range);
        /// The players within `range` of `speaker` hear its `text` as `msgtype` in `language`, each in
        /// its own locale, naming `target`.
        virtual void MonsterTextAround(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, float range);
        /// `listeners`, in order, hear `speaker`'s `text` as `msgtype` in `language`, each in its own
        /// locale, naming `target`.
        virtual void MonsterTextTo(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, std::vector<Player const*> const& listeners);
        /// The players within `range` of `speaker`, of its own team only when `ownTeamOnly` is true,
        /// and the speaker itself hear it say `text` as `msgtype` in `language`.
        virtual void Said(Player& speaker, ChatMsg msgtype, std::string const& text, uint32 language, float range, bool ownTeamOnly);
        /// `receiver` hears `speaker` whisper `text` in `language`; `speaker` is told the whisper went
        /// out, after its own copy of it for an addon message.
        virtual void Whispered(Player& speaker, Player& receiver, std::string const& text, uint32 language);
        /// `speaker` is told `receiver`'s automatic reply as `msgtype`.
        virtual void AutoReplied(Player& speaker, Player const& receiver, ChatMsg msgtype);

        /// The installed notices, or the base when none are installed.
        static ChatNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static ChatNotices* Install(ChatNotices* notices);
};

#define sChatNotices ChatNotices::Installed()

#endif
