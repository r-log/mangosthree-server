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

#ifndef MANGOS_H_AURANOTICES
#define MANGOS_H_AURANOTICES

class Player;
class SpellAuraHolder;
class Unit;

/**
 * What a unit's auras tell the clients, one call per notice: the units around an aura's target hear
 * the aura change, and a player the visible auras of a unit it comes to see. The session installs
 * the notices that send them at start-up; this base sends nothing.
 */
class AuraNotices
{
    public:
        virtual ~AuraNotices();

        /// The units around `holder`'s target, the target among them, are told the aura changed, or
        /// left its slot when `removed` is true.
        virtual void AuraUpdate(SpellAuraHolder const& holder, bool removed);
        /// `viewer` is told every visible aura of `target`.
        virtual void VisibleAuras(Player& viewer, Unit const& target);

        /// The installed notices, or the base when none are installed.
        static AuraNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static AuraNotices* Install(AuraNotices* notices);
};

#define sAuraNotices AuraNotices::Installed()

#endif
