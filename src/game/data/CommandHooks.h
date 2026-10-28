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

#ifndef MANGOS_H_COMMANDHOOKS
#define MANGOS_H_COMMANDHOOKS

class Player;

/**
 * What the session and the domain ask of the GM command system, which sits above them
 * (`ChatCommands/`, `Chat.cpp`): whether a player may use a command, which lets a GM past an NPC's
 * checks, and the run of a command typed in chat. mangosd installs the command system's hooks
 * (`ChatHandler::GetCommandHooks`) at start-up; the core calls the installed ones through
 * sCommandHooks. This base answers for a core with no command system: a player may use no
 * command, and a line that starts like a command ('.' or '!') is taken as handled, so it is
 * dropped rather than sent as chat.
 */
class CommandHooks
{
    public:
        virtual ~CommandHooks();

        /// Whether `player` may use the command `command` names, such as "auction" or "npc tame".
        virtual bool MayUseCommand(Player& player, char const* command);
        /// Runs the command `player` typed in chat. False when `text` is not one, and it then goes
        /// out as chat.
        virtual bool ParseChatCommand(Player& player, char const* text);

        /// The installed hooks, or the base when none are installed.
        static CommandHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the listener opens.
        static CommandHooks* Install(CommandHooks* hooks);
        /// Whether hooks are installed.
        static bool IsInstalled();
};

#define sCommandHooks CommandHooks::Installed()

#endif
