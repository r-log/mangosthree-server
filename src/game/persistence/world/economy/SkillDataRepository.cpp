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

#include "SkillDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace SkillDataRepository
{
    static SkillDiscoveryRow ReadSkillDiscovery(Field* fields)
    {
        SkillDiscoveryRow row;
        row.spellId = fields[0].GetUInt32();
        row.reqSpell = fields[1].GetInt32();
        row.reqSkillValue = fields[2].GetInt32();
        row.chance = fields[3].GetFloat();
        return row;
    }

    std::vector<SkillDiscoveryRow> QueryDiscoveries()
    {
        //                                  0          1           2                3
        QueryResult* result = WorldDatabase.Query("SELECT `spellId`, `reqSpell`, `reqSkillValue`, `chance` FROM `skill_discovery_template`");
        return ReadRows<SkillDiscoveryRow>(result, ReadSkillDiscovery);
    }

    static SkillExtraItemRow ReadSkillExtraItem(Field* fields)
    {
        SkillExtraItemRow row;
        row.spellId = fields[0].GetUInt32();
        row.requiredSpecialization = fields[1].GetUInt32();
        row.additionalCreateChance = fields[2].GetFloat();
        row.additionalMaxNum = fields[3].GetUInt8();
        return row;
    }

    std::vector<SkillExtraItemRow> QueryExtraItems()
    {
        //                                  0        1                       2                       3
        QueryResult* result = WorldDatabase.Query("SELECT `spellId`, `requiredSpecialization`, `additionalCreateChance`, `additionalMaxNum` FROM `skill_extra_item_template`");
        return ReadRows<SkillExtraItemRow>(result, ReadSkillExtraItem);
    }

    static FishingBaseLevelRow ReadFishingBaseLevel(Field* fields)
    {
        FishingBaseLevelRow row;
        row.entry = fields[0].GetUInt32();
        row.skill = fields[1].GetInt32();
        return row;
    }

    std::vector<FishingBaseLevelRow> QueryFishingBaseLevels()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`skill` FROM `skill_fishing_base_level`");
        return ReadRows<FishingBaseLevelRow>(result, ReadFishingBaseLevel);
    }
}
