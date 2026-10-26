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
#include "persistence/RowReader.h"

namespace AchievementDataRepository
{
    static AchievementCriteriaRequirementRow ReadAchievementCriteriaRequirement(Field* fields)
    {
        AchievementCriteriaRequirementRow row;
        row.criteriaId = fields[0].GetUInt32();
        row.type = fields[1].GetUInt32();
        row.value1 = fields[2].GetUInt32();
        row.value2 = fields[3].GetUInt32();
        return row;
    }

    std::vector<AchievementCriteriaRequirementRow> QueryCriteriaRequirements()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `criteria_id`, `type`, `value1`, `value2` FROM `achievement_criteria_requirement`");
        return ReadRows<AchievementCriteriaRequirementRow>(result, ReadAchievementCriteriaRequirement);
    }

    static AchievementRewardRow ReadAchievementReward(Field* fields)
    {
        AchievementRewardRow row;
        row.entry = fields[0].GetUInt32();
        row.gender = fields[1].GetUInt8();
        row.titleA = fields[2].GetUInt32();
        row.titleH = fields[3].GetUInt32();
        row.item = fields[4].GetUInt32();
        row.sender = fields[5].GetUInt32();
        row.subject = fields[6].GetCppString();
        row.text = fields[7].GetCppString();
        return row;
    }

    std::vector<AchievementRewardRow> QueryRewards()
    {
        //                                0        1         2          3          4       5         6          7
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `gender`, `title_A`, `title_H`, `item`, `sender`, `subject`, `text` FROM `achievement_reward`");
        return ReadRows<AchievementRewardRow>(result, ReadAchievementReward);
    }

    static AchievementRewardLocaleRow ReadAchievementRewardLocale(Field* fields)
    {
        AchievementRewardLocaleRow row;
        row.entry = fields[0].GetUInt32();
        row.gender = fields[1].GetUInt8();
        for (int i = 0; i < ACHIEVEMENT_REWARD_LOCALE_ROW_LOCALES; ++i)
        {
            row.subject[i] = fields[2 + 2 * i].GetCppString();
            row.text[i] = fields[3 + 2 * i].GetCppString();
        }
        return row;
    }

    std::vector<AchievementRewardLocaleRow> QueryRewardLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`gender`,"
                                                  "`subject_loc1`,`text_loc1`,`subject_loc2`,`text_loc2`,"
                                                  "`subject_loc3`,`text_loc3`,`subject_loc4`,`text_loc4`,"
                                                  "`subject_loc5`,`text_loc5`,`subject_loc6`,`text_loc6`,"
                                                  "`subject_loc7`,`text_loc7`,`subject_loc8`,`text_loc8`,"
                                                  "`subject_loc9`,`text_loc9`,`subject_loc10`,`text_loc10`,"
                                                  "`subject_loc11`,`text_loc11`"
                                                  " FROM `locales_achievement_reward`");
        return ReadRows<AchievementRewardLocaleRow>(result, ReadAchievementRewardLocale);
    }
}
