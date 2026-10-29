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

#ifndef MANGOS_H_GROUPHOOKS
#define MANGOS_H_GROUPHOOKS

#include "Platform/Define.h"

class Group;
class Player;
class Unit;

/**
 * What a kill tells the group that tapped the victim: its members' reward. mangosd installs the
 * group's hooks (`Group::GetGroupHooks`) at start-up; the core calls the installed ones through
 * sGroupHooks. This base answers for a core with no groups: nobody is rewarded.
 */
class GroupHooks
{
    public:
        virtual ~GroupHooks();

        /// Rewards `group` for the kill of `victim`. `tap` is the player credited with the kill,
        /// NULL when it is not in the world.
        virtual void RewardAtKill(Group& group, Unit& victim, Player* tap);

        /// The installed hooks, or the base when none are installed.
        static GroupHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the world loads.
        static GroupHooks* Install(GroupHooks* hooks);
        /// Whether hooks are installed.
        static bool IsInstalled();
};

#define sGroupHooks GroupHooks::Installed()

#endif
