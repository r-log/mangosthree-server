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

#ifndef MANGOS_H_CHATPACKETS
#define MANGOS_H_CHATPACKETS

#include "entities/ChatNotices.h"

/// The speakers' chat notices, each built here as a chat message (`BuildMessageChatPacket`): a
/// creature's to the players in range or to its listeners, each localized text built once per
/// locale; a player's to the players in range and to itself, a whisper to its receiver and the
/// replies to its speaker.
class SessionChatNotices : public ChatNotices
{
    public:
        void MonsterSaid(WorldObject const& speaker, ChatMsg msgtype, char const* text, Unit const* target, float range) override;
        void MonsterTextAround(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, float range) override;
        void MonsterTextTo(WorldObject const& speaker, MangosStringLocale const& text, ChatMsg msgtype, Language language, Unit const* target, std::vector<Player const*> const& listeners) override;
        void Said(Player& speaker, ChatMsg msgtype, std::string const& text, uint32 language, float range, bool ownTeamOnly) override;
        void Whispered(Player& speaker, Player& receiver, std::string const& text, uint32 language) override;
        void AutoReplied(Player& speaker, Player const& receiver, ChatMsg msgtype) override;
};

#endif
