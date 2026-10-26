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

#include "DungeonDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace DungeonDataRepository
{
    static InstanceEncounterRow ReadInstanceEncounter(Field* fields)
    {
        InstanceEncounterRow row;
        row.entry = fields[0].GetUInt32();
        row.creditType = fields[1].GetUInt8();
        row.creditEntry = fields[2].GetUInt32();
        row.lastEncounterDungeon = fields[3].GetUInt32();
        return row;
    }

    std::vector<InstanceEncounterRow> QueryInstanceEncounters()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `creditType`, `creditEntry`, `lastEncounterDungeon` FROM `instance_encounters`");
        return ReadRows<InstanceEncounterRow>(result, ReadInstanceEncounter);
    }

    static DungeonFinderRequirementRow ReadDungeonFinderRequirement(Field* fields)
    {
        DungeonFinderRequirementRow row;
        row.mapId = fields[0].GetUInt32();
        row.difficulty = fields[1].GetUInt32();
        row.minItemLevel = fields[2].GetUInt32();
        row.item = fields[3].GetUInt32();
        row.item2 = fields[4].GetUInt32();
        row.allianceQuest = fields[5].GetUInt32();
        row.hordeQuest = fields[6].GetUInt32();
        row.achievement = fields[7].GetUInt32();
        if (char const* text = fields[8].GetString())
        {
            row.questIncompleteText = text;
        }
        return row;
    }

    std::vector<DungeonFinderRequirementRow> QueryDungeonFinderRequirements()
    {
        //                                 0      1           2               3     4       5               6            7            8
        QueryResult* result = WorldDatabase.Query("SELECT `mapId`, `difficulty`, `min_item_level`, `item`, `item_2`, `alliance_quest`, `horde_quest`, `achievement`, `quest_incomplete_text` FROM `dungeonfinder_requirements`");
        return ReadRows<DungeonFinderRequirementRow>(result, ReadDungeonFinderRequirement);
    }

    static DungeonFinderRewardRow ReadDungeonFinderReward(Field* fields)
    {
        DungeonFinderRewardRow row;
        row.level = fields[1].GetUInt32();
        row.baseXpReward = fields[2].GetUInt32();
        row.baseMonetaryReward = fields[3].GetInt32();
        return row;
    }

    std::vector<DungeonFinderRewardRow> QueryDungeonFinderRewards()
    {
        //                                  0     1        2                 3
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `level`, `base_xp_reward`, `base_monetary_reward` FROM `dungeonfinder_rewards`");
        return ReadRows<DungeonFinderRewardRow>(result, ReadDungeonFinderReward);
    }

    static DungeonFinderItemRewardRow ReadDungeonFinderItemReward(Field* fields)
    {
        DungeonFinderItemRewardRow row;
        row.id = fields[0].GetUInt32();
        row.minLevel = fields[1].GetUInt32();
        row.maxLevel = fields[2].GetUInt32();
        row.itemReward = fields[3].GetInt32();
        row.itemAmount = fields[4].GetUInt32();
        row.dungeonType = fields[5].GetUInt32();
        return row;
    }

    std::vector<DungeonFinderItemRewardRow> QueryDungeonFinderItemRewards()
    {
        //                                  0     1            2            3              4              5
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `min_level`, `max_level`, `item_reward`, `item_amount`, `dungeon_type` FROM `dungeonfinder_item_rewards`");
        return ReadRows<DungeonFinderItemRewardRow>(result, ReadDungeonFinderItemReward);
    }

    static DungeonEntranceRow ReadDungeonEntrance(Field* fields)
    {
        DungeonEntranceRow row;
        row.dungeonId = fields[0].GetUInt32();
        row.positionX = fields[1].GetFloat();
        row.positionY = fields[2].GetFloat();
        row.positionZ = fields[3].GetFloat();
        row.orientation = fields[4].GetFloat();
        return row;
    }

    std::vector<DungeonEntranceRow> QueryDungeonEntrances()
    {
        //                                  0            1             2             3             4
        QueryResult* result = WorldDatabase.Query("SELECT `dungeonId`, `position_x`, `position_y`, `position_z`, `orientation` FROM `lfg_dungeon_entrances`");
        return ReadRows<DungeonEntranceRow>(result, ReadDungeonEntrance);
    }
}
