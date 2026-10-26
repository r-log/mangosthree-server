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

namespace DungeonDataRepository
{
    QueryResult* QueryInstanceEncounters()
    {
        return WorldDatabase.Query("SELECT `entry`, `creditType`, `creditEntry`, `lastEncounterDungeon` FROM `instance_encounters`");
    }

    QueryResult* QueryDungeonFinderRequirements()
    {
        //                                 0      1           2               3     4       5               6            7            8
        return WorldDatabase.Query("SELECT `mapId`, `difficulty`, `min_item_level`, `item`, `item_2`, `alliance_quest`, `horde_quest`, `achievement`, `quest_incomplete_text` FROM `dungeonfinder_requirements`");
    }

    QueryResult* QueryDungeonFinderRewards()
    {
        //                                  0     1        2                 3
        return WorldDatabase.Query("SELECT `id`, `level`, `base_xp_reward`, `base_monetary_reward` FROM `dungeonfinder_rewards`");
    }

    QueryResult* QueryDungeonFinderItemRewards()
    {
        //                                  0     1            2            3              4              5
        return WorldDatabase.Query("SELECT `id`, `min_level`, `max_level`, `item_reward`, `item_amount`, `dungeon_type` FROM `dungeonfinder_item_rewards`");
    }

    QueryResult* QueryDungeonEntrances()
    {
        //                                  0            1             2             3             4
        return WorldDatabase.Query("SELECT `dungeonId`, `position_x`, `position_y`, `position_z`, `orientation` FROM `lfg_dungeon_entrances`");
    }
}
