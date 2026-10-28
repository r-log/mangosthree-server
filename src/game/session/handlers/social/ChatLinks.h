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

#ifndef MANGOS_H_CHATLINKS
#define MANGOS_H_CHATLINKS

/// The shift-link check the chat handlers run on a player's message when
/// ChatStrictLinkChecking.Severity is set and the player is below a moderator.
struct ChatLinks
{
    public:
        /// Whether `message` is at most 255 bytes and its links are well formed. Severity 1 accepts
        /// only the |c, |H, |h, |r and || commands; 2 also wants them in the order |c |H |h |h |r; 3
        /// parses every link and checks it against the item, quest, spell, talent, enchant,
        /// achievement, glyph and currency data, the item's colour and its name in the client's
        /// locales included.
        static bool IsValid(const char* message);
};

#endif
