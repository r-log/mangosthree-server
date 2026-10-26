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
#include "persistence/RowReader.h"

#include <sstream>
#include <string>

namespace CreatureDataRepository
{
    static ClassLevelStatsRow ReadClassLevelStats(Field* fields, int maxExpansion)
    {
        ClassLevelStatsRow row;
        row.classId = fields[0].GetUInt32();
        row.level = fields[1].GetUInt32();
        row.baseMana = fields[2].GetUInt32();
        row.baseMeleeAttackPower = fields[3].GetFloat();
        row.baseRangedAttackPower = fields[4].GetFloat();
        row.baseArmor = fields[5].GetUInt32();
        for (int i = 0; i <= maxExpansion; ++i)
        {
            row.baseHealth.push_back(fields[6 + (i * 2)].GetUInt32());
            row.baseDamage.push_back(fields[7 + (i * 2)].GetFloat());
        }
        return row;
    }

    std::vector<ClassLevelStatsRow> QueryClassLevelStats(int maxExpansion)
    {
        std::string queryStr = "SELECT `Class`, `Level`, `BaseMana`, `BaseMeleeAttackPower`, `BaseRangedAttackPower`, `BaseArmor`";

        for (int i = 0; i <= maxExpansion; i++)
        {
            std::ostringstream str;
            str << ", `BaseHealthExp" << i << "`, `BaseDamageExp" << i << "`";
            queryStr.append(str.str().c_str());
        }

        queryStr.append(" FROM `creature_template_classlevelstats` ORDER BY `Class`, `Level`");

        QueryResult* result = WorldDatabase.Query(queryStr.c_str());
        return ReadRows<ClassLevelStatsRow>(result, [maxExpansion](Field* fields) { return ReadClassLevelStats(fields, maxExpansion); });
    }

    static CreatureModelRaceRow ReadCreatureModelRace(Field* fields)
    {
        CreatureModelRaceRow row;
        row.modelid = fields[0].GetUInt32();
        row.racemask = fields[1].GetUInt32();
        row.creatureEntry = fields[2].GetUInt32();
        row.modelidRacial = fields[3].GetUInt32();
        return row;
    }

    std::vector<CreatureModelRaceRow> QueryModelRaces()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `modelid`, `racemask`, `creature_entry`, `modelid_racial` FROM `creature_model_race`");
        return ReadRows<CreatureModelRaceRow>(result, ReadCreatureModelRace);
    }

    static PetNamePartRow ReadPetNamePart(Field* fields)
    {
        PetNamePartRow row;
        row.word = fields[0].GetCppString();
        row.entry = fields[1].GetUInt32();
        row.half = fields[2].GetBool();
        return row;
    }

    std::vector<PetNamePartRow> QueryPetNameParts()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `word`,`entry`,`half` FROM `pet_name_generation`");
        return ReadRows<PetNamePartRow>(result, ReadPetNamePart);
    }

    static PetLevelStatsRow ReadPetLevelStats(Field* fields)
    {
        PetLevelStatsRow row;
        row.creatureEntry = fields[0].GetUInt32();
        row.level = fields[1].GetUInt32();
        row.hp = fields[2].GetUInt16();
        row.mana = fields[3].GetUInt16();
        for (int i = 0; i < PET_LEVEL_STATS_ROW_STATS; ++i)
        {
            row.stats[i] = fields[i + 4].GetUInt16();
        }
        row.armor = fields[9].GetUInt16();
        return row;
    }

    std::vector<PetLevelStatsRow> QueryPetLevelStats()
    {
        //                                 0               1      2   3     4    5    6    7     8    9
        QueryResult* result = WorldDatabase.Query("SELECT `creature_entry`, `level`, `hp`, `mana`, `str`, `agi`, `sta`, `inte`, `spi`, `armor` FROM `pet_levelstats`");
        return ReadRows<PetLevelStatsRow>(result, ReadPetLevelStats);
    }

    static CreatureLinkingTemplateRow ReadCreatureLinkingTemplate(Field* fields)
    {
        CreatureLinkingTemplateRow row;
        row.entry = fields[0].GetUInt32();
        row.map = fields[1].GetUInt32();
        row.masterEntry = fields[2].GetUInt32();
        row.flag = fields[3].GetUInt16();
        row.searchRange = fields[4].GetUInt16();
        return row;
    }

    std::vector<CreatureLinkingTemplateRow> QueryLinkingTemplates()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `map`, `master_entry`, `flag`, `search_range` FROM `creature_linking_template`");
        return ReadRows<CreatureLinkingTemplateRow>(result, ReadCreatureLinkingTemplate);
    }

    static CreatureLinkingRow ReadCreatureLinking(Field* fields)
    {
        CreatureLinkingRow row;
        row.guid = fields[0].GetUInt32();
        row.masterGuid = fields[1].GetUInt32();
        row.flag = fields[2].GetUInt16();
        return row;
    }

    std::vector<CreatureLinkingRow> QueryLinkings()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `guid`, `master_guid`, `flag` FROM `creature_linking`");
        return ReadRows<CreatureLinkingRow>(result, ReadCreatureLinking);
    }

    void SetNpcFlags(uint32 npcFlags, uint32 entry)
    {
        WorldDatabase.PExecuteLog("UPDATE `creature_template` SET `NpcFlags` = '%u' WHERE `entry` = '%u'", npcFlags, entry);
    }

    void SetFaction(uint32 factionId, uint32 entry)
    {
        WorldDatabase.PExecuteLog("UPDATE `creature_template` SET `FactionAlliance` = '%u', `FactionHorde` = '%u' WHERE `entry` = '%u'", factionId, factionId, entry);
    }
}
