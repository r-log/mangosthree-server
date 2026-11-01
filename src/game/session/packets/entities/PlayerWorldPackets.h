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

#ifndef MANGOS_H_PLAYERWORLDPACKETS
#define MANGOS_H_PLAYERWORLDPACKETS

#include "entities/player/world/PlayerWorldNotices.h"

/// A player's world notices, each built here: to the player's own client, the environment's damage
/// and its drunkenness to the units around it.
class SessionPlayerWorldNotices : public PlayerWorldNotices
{
    public:
        void DungeonDifficultyShown(Player const& player, uint32 difficulty, bool inGroup) override;
        void RaidDifficultyShown(Player const& player, uint32 difficulty, bool inGroup) override;
        void InstanceResetFailedNotify(Player const& player, uint32 mapId) override;
        void InstanceReset(Player const& player, uint32 mapId) override;
        void InstanceResetFailed(Player const& player, uint32 reason, uint32 mapId) override;
        void RaidInstanceMessage(Player const& player, uint32 type, uint32 mapId, Difficulty difficulty, uint32 time) override;
        void RaidInfoShown(Player const& player, std::vector<DungeonPersistentState*> const& permBinds) override;
        void SavedInstancesShown(Player const& player, std::vector<uint32> const& permMapIds) override;
        void MirrorTimerStarted(Player const& player, MirrorTimerType type, uint32 max, uint32 current, int32 regen) override;
        void MirrorTimerStopped(Player const& player, MirrorTimerType type) override;
        void EnvironmentalDamageLogged(Player const& player, EnviromentalDamage type, uint32 damage, uint32 absorb, uint32 resist) override;
        void InebriationThresholdCrossed(Player const& player, uint32 state, uint32 itemId) override;
        void NewTaxiPath(Player const& player) override;
        void RaidGroupOnly(Player const& player, uint32 timerMs, uint32 error) override;
        void PvpRewardsShown(Player const& player) override;
        void RatedBgStatsShown(Player const& player) override;
};

#endif
