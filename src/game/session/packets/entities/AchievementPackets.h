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

#ifndef MANGOS_H_ACHIEVEMENTPACKETS
#define MANGOS_H_ACHIEVEMENTPACKETS

#include "AchievementMgr.h"

class WorldPacket;

/// Initializes `packet` as `earner`'s achievement `achievementId` earned now: the earner's packed guid,
/// the achievement, the date and a zero word.
void BuildAchievementEarnedPacket(WorldPacket& packet, Player const& earner, uint32 achievementId);
/// Initializes `packet` as the progress of `player`'s criteria `criteriaId`: the criteria, the counter
/// packed like a guid, the player's packed guid, whether its time ran out, the date and the time spent
/// twice.
void BuildCriteriaUpdatePacket(WorldPacket& packet, Player const& player, uint32 criteriaId, CriteriaProgress const& progress);
/// Initializes `packet` as the loss of the achievement `achievementId`.
void BuildAchievementDeletedPacket(WorldPacket& packet, uint32 achievementId);
/// Initializes `packet` as the loss of the criteria `criteriaId`'s progress.
void BuildCriteriaDeletedPacket(WorldPacket& packet, uint32 criteriaId);
/// Initializes `packet` as every criteria's progress and every achievement `player` completed.
void BuildAllAchievementDataPacket(WorldPacket& packet, Player const& player);
/// Initializes `packet` as `inspected`'s achievements and criteria progress for an inspecting player.
void BuildRespondInspectAchievementsPacket(WorldPacket& packet, Player const& inspected);
/// Initializes `packet` as the realm's first of the achievement `achievementId`, earned by `name`
/// (`guid`).
void BuildServerFirstAchievementPacket(WorldPacket& packet, char const* name, ObjectGuid guid, uint32 achievementId);

/// Sends `viewer` the achievements of `inspected`.
void SendRespondInspectAchievements(Player& viewer, Player const& inspected);

/// The achievements' notices, each built here: the achievement chat to its listeners, each in its own
/// locale, the earned achievement to the units around the earner, the rest to the player's own client.
class SessionAchievementNotices : public AchievementNotices
{
    public:
        void EarnedSaid(Player const& earner, ChatMsg msgtype, uint32 achievementId, std::vector<Player const*> const& listeners) override;
        void Earned(Player& earner, uint32 achievementId, float range) override;
        void CriteriaUpdated(Player& player, uint32 criteriaId, CriteriaProgress const& progress) override;
        void CriteriaDeleted(Player& player, uint32 criteriaId) override;
        void AchievementDeleted(Player& player, uint32 achievementId) override;
        void AllData(Player& player) override;
};

#endif
