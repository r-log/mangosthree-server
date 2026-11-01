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

#ifndef MANGOS_H_PLAYERCOMBATNOTICES
#define MANGOS_H_PLAYERCOMBATNOTICES

#include "Player.h"

/**
 * What a player's fights, deaths and duels tell its own client, one call per notice, and a duel's
 * outcome to the units around it. The player keeps when each is told. The session installs the
 * notices that send them at start-up; this base sends nothing.
 */
class PlayerCombatNotices
{
    public:
        virtual ~PlayerCombatNotices();

        /// `player` is told its target is dead.
        virtual void SwingDeadTarget(Player const& player);
        /// `player` is told it cannot attack its target.
        virtual void SwingCantAttack(Player const& player);
        /// `player` is told it has `points` combo points on `target`.
        virtual void ComboPointsUpdated(Player const& player, Unit const& target, uint8 points);
        /// `player` is told it is about to become a ghost.
        virtual void PreResurrect(Player const& player);
        /// `player`'s minimap stops showing the spirit healer.
        virtual void SpiritHealerLocCleared(Player const& player);
        /// `player`'s minimap shows the spirit healer at `x`, `y`, `z` on the map `mapId`.
        virtual void SpiritHealerLocShown(Player const& player, uint32 mapId, float x, float y, float z);
        /// `player` is told it may reclaim its corpse in `delayMs` milliseconds.
        virtual void CorpseReclaimDelay(Player const& player, uint32 delayMs);
        /// `player` is told it left the duel's bounds.
        virtual void DuelOutOfBounds(Player const& player);
        /// `player` is told it is back in the duel's bounds.
        virtual void DuelInBounds(Player const& player);
        /// `player` and `opponent` are told their duel ended as `type`; the units around `player`, `player`
        /// among them, are told who won unless it was interrupted.
        virtual void DuelCompleted(Player const& player, Player const& opponent, DuelCompleteType type);
        /// `player` is told its duel starts in `seconds` seconds.
        virtual void DuelCountdown(Player const& player, uint32 seconds);

        /// The installed notices, or the base when none are installed.
        static PlayerCombatNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static PlayerCombatNotices* Install(PlayerCombatNotices* notices);
};

#define sPlayerCombatNotices PlayerCombatNotices::Installed()

#endif
