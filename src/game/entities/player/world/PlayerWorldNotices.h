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

#ifndef MANGOS_H_PLAYERWORLDNOTICES
#define MANGOS_H_PLAYERWORLDNOTICES

#include "Player.h"

#include <vector>

class DungeonPersistentState;

/**
 * What a player's place in the world tells its own client, one call per notice: its instances and
 * their resets, its timers, the environment's damage and its drunkenness to the units around it, a
 * new taxi path, the raid-group reminder and its battleground statistics. The player keeps when each
 * is told. The session installs the notices that send them at start-up; this base sends nothing.
 */
class PlayerWorldNotices
{
    public:
        virtual ~PlayerWorldNotices();

        /// `player` is told its dungeon difficulty `difficulty`, in a group when `inGroup`.
        virtual void DungeonDifficultyShown(Player const& player, uint32 difficulty, bool inGroup);
        /// `player` is told its raid difficulty `difficulty`, in a group when `inGroup`.
        virtual void RaidDifficultyShown(Player const& player, uint32 difficulty, bool inGroup);
        /// `player` is told the instance of the map `mapId` failed to reset.
        virtual void InstanceResetFailedNotify(Player const& player, uint32 mapId);
        /// `player` is told its instance of the map `mapId` was reset.
        virtual void InstanceReset(Player const& player, uint32 mapId);
        /// `player` is told its instance of the map `mapId` could not be reset for `reason`.
        virtual void InstanceResetFailed(Player const& player, uint32 reason, uint32 mapId);
        /// `player` is told the raid instance message `type` for the map `mapId` at `difficulty`, `time`
        /// seconds before its reset.
        virtual void RaidInstanceMessage(Player const& player, uint32 type, uint32 mapId, Difficulty difficulty, uint32 time);
        /// `player` is told its permanent instance binds `permBinds`.
        virtual void RaidInfoShown(Player const& player, std::vector<DungeonPersistentState*> const& permBinds);
        /// `player` is told whether it has saved instances, then the map of each of `permMapIds`.
        virtual void SavedInstancesShown(Player const& player, std::vector<uint32> const& permMapIds);
        /// `player` is shown its mirror timer `type` at `current` of `max`, changing by `regen`.
        virtual void MirrorTimerStarted(Player const& player, MirrorTimerType type, uint32 max, uint32 current, int32 regen);
        /// `player`'s mirror timer `type` is stopped.
        virtual void MirrorTimerStopped(Player const& player, MirrorTimerType type);
        /// The units around `player`, `player` among them, are told it took `damage` of the environmental
        /// `type`, `absorb` of it absorbed and `resist` resisted.
        virtual void EnvironmentalDamageLogged(Player const& player, EnviromentalDamage type, uint32 damage, uint32 absorb, uint32 resist);
        /// The units around `player`, `player` among them, are told it crossed into the drunken state
        /// `state`, from the item `itemId`.
        virtual void InebriationThresholdCrossed(Player const& player, uint32 state, uint32 itemId);
        /// `player` is told it learned a new taxi path.
        virtual void NewTaxiPath(Player const& player);
        /// `player` is told it is sent home in `timerMs` milliseconds for not being in a raid group, `error`
        /// when the timer is 0.
        virtual void RaidGroupOnly(Player const& player, uint32 timerMs, uint32 error);
        /// `player` is told its conquest caps and earnings.
        virtual void PvpRewardsShown(Player const& player);
        /// `player` is told its rated battleground statistics.
        virtual void RatedBgStatsShown(Player const& player);

        /// The installed notices, or the base when none are installed.
        static PlayerWorldNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static PlayerWorldNotices* Install(PlayerWorldNotices* notices);
};

#define sPlayerWorldNotices PlayerWorldNotices::Installed()

#endif
