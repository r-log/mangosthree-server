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

#ifndef MANGOS_H_OUTDOORPVPHOOKS
#define MANGOS_H_OUTDOORPVPHOOKS

#include "Platform/Define.h"

class Creature;
class Player;

/**
 * What a death tells the outdoor PvP script of the zone it happens in: a player's death and its
 * killer, and a creature's death. mangosd installs the outdoor PvP manager's hooks
 * (`OutdoorPvPMgr::GetOutdoorPvPHooks`) at start-up; the core calls the installed ones through
 * sOutdoorPvPHooks. This base answers for a core with no outdoor PvP scripts: nothing is told.
 */
class OutdoorPvPHooks
{
    public:
        virtual ~OutdoorPvPHooks();

        /// Tells the script of zone `zone`, if it has one, that `killer` killed `victim`.
        virtual void PlayerKilled(uint32 zone, Player& killer, Player& victim);
        /// Tells the script of zone `zone`, if it has one, that `victim` died.
        virtual void CreatureDied(uint32 zone, Creature& victim);

        /// The installed hooks, or the base when none are installed.
        static OutdoorPvPHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the world loads.
        static OutdoorPvPHooks* Install(OutdoorPvPHooks* hooks);
        /// Whether hooks are installed.
        static bool IsInstalled();
};

#define sOutdoorPvPHooks OutdoorPvPHooks::Installed()

#endif
