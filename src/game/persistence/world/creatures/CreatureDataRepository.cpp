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

#include "CreatureDataRepository.h"
#include "Database/DatabaseEnv.h"

#include <sstream>
#include <string>

namespace CreatureDataRepository
{
    QueryResult* QueryClassLevelStats(int maxExpansion)
    {
        std::string queryStr = "SELECT `Class`, `Level`, `BaseMana`, `BaseMeleeAttackPower`, `BaseRangedAttackPower`, `BaseArmor`";

        for (int i = 0; i <= maxExpansion; i++)
        {
            std::ostringstream str;
            str << ", `BaseHealthExp" << i << "`, `BaseDamageExp" << i << "`";
            queryStr.append(str.str().c_str());
        }

        queryStr.append(" FROM `creature_template_classlevelstats` ORDER BY `Class`, `Level`");

        return WorldDatabase.Query(queryStr.c_str());
    }

    QueryResult* QueryModelRaces()
    {
        return WorldDatabase.Query("SELECT `modelid`, `racemask`, `creature_entry`, `modelid_racial` FROM `creature_model_race`");
    }

    QueryResult* QueryPetNameParts()
    {
        return WorldDatabase.Query("SELECT `word`,`entry`,`half` FROM `pet_name_generation`");
    }

    QueryResult* QueryPetLevelStats()
    {
        //                                 0               1      2   3     4    5    6    7     8    9
        return WorldDatabase.Query("SELECT `creature_entry`, `level`, `hp`, `mana`, `str`, `agi`, `sta`, `inte`, `spi`, `armor` FROM `pet_levelstats`");
    }

    QueryResult* QueryLinkingTemplates()
    {
        return WorldDatabase.Query("SELECT `entry`, `map`, `master_entry`, `flag`, `search_range` FROM `creature_linking_template`");
    }

    QueryResult* QueryLinkings()
    {
        return WorldDatabase.Query("SELECT `guid`, `master_guid`, `flag` FROM `creature_linking`");
    }
}
