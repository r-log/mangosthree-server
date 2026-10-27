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

#ifndef MANGOS_H_ACHIEVEMENTREPOSITORY
#define MANGOS_H_ACHIEVEMENTREPOSITORY

#include "Platform/Define.h"

#include <vector>

/// One achievement some character has completed.
struct CompletedAchievementRow
{
    uint32 achievement = 0;
};

/// One criteria some character has progress in.
struct AchievementCriteriaProgressRow
{
    uint32 criteria = 0;
};

/// The characters' completed achievements and criteria progress. The two realm-wide reads run at
/// startup, synchronously; every write is queued on the character database, and one called inside
/// CharacterTransaction joins it.
namespace AchievementRepository
{
    /// Selects every achievement some character has completed, once each.
    std::vector<CompletedAchievementRow> QueryCompletedAchievements();

    /// Selects every criteria some character has progress in, once each.
    std::vector<AchievementCriteriaProgressRow> QueryProgressCriteria();

    /// Deletes a character's achievements and criteria progress, in one transaction.
    void DeleteAllOf(uint32 guid);

    /// Writes a character's completed achievement over the old row: a DELETE, then the INSERT.
    void ReplaceCompleted(uint32 guid, uint32 achievement, uint64 date);

    /// Deletes a character's completed achievement.
    void DeleteCompleted(uint32 guid, uint32 achievement);

    /// Each writes a character's progress in one criteria.
    void DeleteProgress(uint32 guid, uint32 criteria);
    void InsertProgress(uint32 guid, uint32 criteria, uint32 counter, uint64 date);

}

#endif
