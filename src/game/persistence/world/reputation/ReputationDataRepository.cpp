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

#include "ReputationDataRepository.h"
#include "Database/DatabaseEnv.h"

namespace ReputationDataRepository
{
    QueryResult* QueryRewardRates()
    {
        return WorldDatabase.Query("SELECT `faction`, `quest_rate`, `creature_rate`, `spell_rate` FROM `reputation_reward_rate`");
    }

    QueryResult* QueryOnKillReputations()
    {
        //                                 0            1                     2
        return WorldDatabase.Query("SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,"
                                   //   3             4             5                   6             7             8                   9
                                   "`IsTeamAward1`, `MaxStanding1`, `RewOnKillRepValue1`, `IsTeamAward2`, `MaxStanding2`, `RewOnKillRepValue2`, `TeamDependent` "
                                   "FROM `creature_onkill_reputation`");
    }

    QueryResult* QuerySpillovers()
    {
        return WorldDatabase.Query("SELECT `faction`, `faction1`, `rate_1`, `rank_1`, `faction2`, `rate_2`, `rank_2`, `faction3`, `rate_3`, `rank_3`, `faction4`, `rate_4`, `rank_4`, `faction5`, `rate_5`, `rank_5` FROM `reputation_spillover_template`");
    }
}
