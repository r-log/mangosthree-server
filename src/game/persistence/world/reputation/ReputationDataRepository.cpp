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
#include "persistence/RowReader.h"

namespace ReputationDataRepository
{
    static ReputationRewardRateRow ReadReputationRewardRate(Field* fields)
    {
        ReputationRewardRateRow row;
        row.faction = fields[0].GetUInt32();
        row.questRate = fields[1].GetFloat();
        row.creatureRate = fields[2].GetFloat();
        row.spellRate = fields[3].GetFloat();
        return row;
    }

    std::vector<ReputationRewardRateRow> QueryRewardRates()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `faction`, `quest_rate`, `creature_rate`, `spell_rate` FROM `reputation_reward_rate`");
        return ReadRows<ReputationRewardRateRow>(result, ReadReputationRewardRate);
    }

    static OnKillReputationRow ReadOnKillReputation(Field* fields)
    {
        OnKillReputationRow row;
        row.creatureId = fields[0].GetUInt32();
        row.rewOnKillRepFaction1 = fields[1].GetUInt32();
        row.rewOnKillRepFaction2 = fields[2].GetUInt32();
        row.isTeamAward1 = fields[3].GetBool();
        row.maxStanding1 = fields[4].GetUInt32();
        row.rewOnKillRepValue1 = fields[5].GetInt32();
        row.isTeamAward2 = fields[6].GetBool();
        row.maxStanding2 = fields[7].GetUInt32();
        row.rewOnKillRepValue2 = fields[8].GetInt32();
        row.teamDependent = fields[9].GetUInt8();
        return row;
    }

    std::vector<OnKillReputationRow> QueryOnKillReputations()
    {
        //                                 0            1                     2
        QueryResult* result = WorldDatabase.Query("SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,"
                                   //   3             4             5                   6             7             8                   9
                                   "`IsTeamAward1`, `MaxStanding1`, `RewOnKillRepValue1`, `IsTeamAward2`, `MaxStanding2`, `RewOnKillRepValue2`, `TeamDependent` "
                                   "FROM `creature_onkill_reputation`");
        return ReadRows<OnKillReputationRow>(result, ReadOnKillReputation);
    }

    static ReputationSpilloverRow ReadReputationSpillover(Field* fields)
    {
        ReputationSpilloverRow row;
        row.faction = fields[0].GetUInt32();
        row.faction1 = fields[1].GetUInt32();
        row.rate1 = fields[2].GetFloat();
        row.rank1 = fields[3].GetUInt32();
        row.faction2 = fields[4].GetUInt32();
        row.rate2 = fields[5].GetFloat();
        row.rank2 = fields[6].GetUInt32();
        row.faction3 = fields[7].GetUInt32();
        row.rate3 = fields[8].GetFloat();
        row.rank3 = fields[9].GetUInt32();
        row.faction4 = fields[10].GetUInt32();
        row.rate4 = fields[11].GetFloat();
        row.rank4 = fields[12].GetUInt32();
        row.faction5 = fields[13].GetUInt32();
        row.rate5 = fields[14].GetFloat();
        row.rank5 = fields[15].GetUInt32();
        return row;
    }

    std::vector<ReputationSpilloverRow> QuerySpillovers()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `faction`, `faction1`, `rate_1`, `rank_1`, `faction2`, `rate_2`, `rank_2`, `faction3`, `rate_3`, `rank_3`, `faction4`, `rate_4`, `rank_4`, `faction5`, `rate_5`, `rank_5` FROM `reputation_spillover_template`");
        return ReadRows<ReputationSpilloverRow>(result, ReadReputationSpillover);
    }
}
