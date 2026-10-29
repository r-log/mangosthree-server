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

#ifndef MANGOS_H_BATTLEGROUNDHOOKS
#define MANGOS_H_BATTLEGROUNDHOOKS

#include "Platform/Define.h"

class Creature;
class Player;

/**
 * What a kill or a heal tells the battleground a player is in: a player's death and its killer, a
 * creature's death and the player credited with it, and a player's healing. mangosd installs the
 * battleground manager's hooks (`BattleGroundMgr::GetBattleGroundHooks`) at start-up; the core
 * calls the installed ones through sBattleGroundHooks. This base answers for a core with no
 * battlegrounds: no player is in one, and nothing is told.
 */
class BattleGroundHooks
{
    public:
        virtual ~BattleGroundHooks();

        /// Tells `victim`'s battleground that `killer` killed it. True when `victim` is in one.
        virtual bool PlayerKilled(Player& victim, Player& killer);
        /// Tells `responsible`'s battleground that it killed `victim`.
        virtual void CreatureKilled(Creature& victim, Player& responsible);
        /// Adds `gain` to `healer`'s healing score in its battleground.
        virtual void HealingDone(Player& healer, uint32 gain);

        /// The installed hooks, or the base when none are installed.
        static BattleGroundHooks& Installed();
        /// Installs `hooks` and answers the hooks it replaces (NULL for the base). NULL installs
        /// the base. Installed once at start-up, before the world loads.
        static BattleGroundHooks* Install(BattleGroundHooks* hooks);
        /// Whether hooks are installed.
        static bool IsInstalled();
};

#define sBattleGroundHooks BattleGroundHooks::Installed()

#endif
