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

#include "AchievementDataRepository.h"
#include "Database/DatabaseEnv.h"

namespace AchievementDataRepository
{
    QueryResult* QueryCriteriaRequirements()
    {
        return WorldDatabase.Query("SELECT `criteria_id`, `type`, `value1`, `value2` FROM `achievement_criteria_requirement`");
    }

    QueryResult* QueryRewards()
    {
        //                                0        1         2          3          4       5         6          7
        return WorldDatabase.Query("SELECT `entry`, `gender`, `title_A`, `title_H`, `item`, `sender`, `subject`, `text` FROM `achievement_reward`");
    }

    QueryResult* QueryRewardLocales()
    {
        return WorldDatabase.Query("SELECT `entry`,`gender`,"
                                   "`subject_loc1`,`text_loc1`,`subject_loc2`,`text_loc2`,"
                                   "`subject_loc3`,`text_loc3`,`subject_loc4`,`text_loc4`,"
                                   "`subject_loc5`,`text_loc5`,`subject_loc6`,`text_loc6`,"
                                   "`subject_loc7`,`text_loc7`,`subject_loc8`,`text_loc8`,"
                                   "`subject_loc9`,`text_loc9`,`subject_loc10`,`text_loc10`,"
                                   "`subject_loc11`,`text_loc11`"
                                   " FROM `locales_achievement_reward`");
    }
}
