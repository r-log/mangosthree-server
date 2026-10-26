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

#ifndef MANGOS_H_ACHIEVEMENTDATAREPOSITORY
#define MANGOS_H_ACHIEVEMENTDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

/// One `achievement_criteria_requirement` row.
struct AchievementCriteriaRequirementRow
{
    uint32 criteriaId = 0;
    uint32 type = 0;
    uint32 value1 = 0;
    uint32 value2 = 0;
};

/// One `achievement_reward` row.
struct AchievementRewardRow
{
    uint32 entry = 0;
    uint8 gender = 0;
    uint32 titleA = 0;
    uint32 titleH = 0;
    uint32 item = 0;
    uint32 sender = 0;
    std::string subject;
    std::string text;
};

enum
{
    /// The locales an achievement reward locale row holds its texts for: every locale after the
    /// default one (`loc1` to `loc11`).
    ACHIEVEMENT_REWARD_LOCALE_ROW_LOCALES = 11
};

/// One `locales_achievement_reward` row: a reward mail's subject and text in each locale, for a
/// gender.
struct AchievementRewardLocaleRow
{
    uint32 entry = 0;
    uint8 gender = 0;
    std::string subject[ACHIEVEMENT_REWARD_LOCALE_ROW_LOCALES];
    std::string text[ACHIEVEMENT_REWARD_LOCALE_ROW_LOCALES];
};

/// The achievement data on the world database (`achievement_criteria_requirement`,
/// `achievement_reward`, `locales_achievement_reward`), read at startup and on a reload,
/// synchronously.
namespace AchievementDataRepository
{
    /// Selects every extra criteria requirement: criteria, type and two values.
    std::vector<AchievementCriteriaRequirementRow> QueryCriteriaRequirements();

    /// Selects every achievement reward: achievement, gender, the two titles, item, sender, mail
    /// subject and text.
    std::vector<AchievementRewardRow> QueryRewards();

    /// Selects every reward's localised mail subject and text, by achievement and gender.
    std::vector<AchievementRewardLocaleRow> QueryRewardLocales();
}

#endif
