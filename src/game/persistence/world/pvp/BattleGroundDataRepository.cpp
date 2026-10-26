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

#include "BattleGroundDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace BattleGroundDataRepository
{
    static BattleGroundTemplateRow ReadBattleGroundTemplate(Field* fields)
    {
        BattleGroundTemplateRow row;
        row.id = fields[0].GetUInt32();
        row.minPlayersPerTeam = fields[1].GetUInt32();
        row.maxPlayersPerTeam = fields[2].GetUInt32();
        row.allianceStartLoc = fields[3].GetUInt32();
        row.allianceStartO = fields[4].GetFloat();
        row.hordeStartLoc = fields[5].GetUInt32();
        row.hordeStartO = fields[6].GetFloat();
        row.startMaxDist = fields[7].GetFloat();
        return row;
    }

    std::vector<BattleGroundTemplateRow> QueryTemplates()
    {
        //                                  0     1                   2                   3                  4                5               6              7
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`");
        return ReadRows<BattleGroundTemplateRow>(result, ReadBattleGroundTemplate);
    }

    static BattleMasterRow ReadBattleMaster(Field* fields)
    {
        BattleMasterRow row;
        row.entry = fields[0].GetUInt32();
        row.bgTemplate = fields[1].GetUInt32();
        return row;
    }

    std::vector<BattleMasterRow> QueryBattleMasters()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`bg_template` FROM `battlemaster_entry`");
        return ReadRows<BattleMasterRow>(result, ReadBattleMaster);
    }

    static BattleGroundEventIndexRow ReadBattleGroundEventIndex(Field* fields)
    {
        BattleGroundEventIndexRow row;
        row.type = fields[0].GetUInt8();
        row.guid = fields[1].GetUInt32();
        row.event1 = fields[2].GetUInt8();
        row.event2 = fields[3].GetUInt8();
        row.map = fields[4].GetUInt32();
        row.spawnGuid = fields[5].GetUInt32();
        row.descriptionMap = fields[6].GetUInt32();
        row.descriptionEvent1 = fields[7].GetUInt8();
        row.descriptionEvent2 = fields[8].GetUInt8();
        if (char const* text = fields[9].GetString())
        {
            row.description = text;
        }
        return row;
    }

    std::vector<BattleGroundEventIndexRow> QueryEventIndexes()
    {
        //                                  0         1           2                3                4              5           6
        QueryResult* result = WorldDatabase.Query("SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1` AS `ev1`, `data`.`ev2` AS ev2, `data`.`map` AS m, `data`.`guid2`, `description`.`map`, "
                                                  //                              7                  8                   9
                                                  "`description`.`event1`, `description`.`event2`, `description`.`description` "
                                                  "FROM "
                                                  "(SELECT '1' AS typ, `a`.`guid` AS `guid1`, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
                                                  "FROM `gameobject_battleground` AS a "
                                                  "LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` "
                                                  "UNION "
                                                  "SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
                                                  "FROM `creature_battleground` AS a "
                                                  "LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` "
                                                  ") data "
                                                  "RIGHT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` "
                                                  "AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` "
                                                  // full outer join doesn't work in mysql :-/ so just UNION-select the same again and add a left outer join
                                                  "UNION "
                                                  "SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1`, `data`.`ev2`, `data`.`map`, `data`.`guid2`, `description`.`map`, "
                                                  "`description`.`event1`, `description`.`event2`, `description`.`description` "
                                                  "FROM "
                                                  "(SELECT '1' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
                                                  "FROM `gameobject_battleground` AS a "
                                                  "LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` "
                                                  "UNION "
                                                  "SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
                                                  "FROM `creature_battleground` AS a "
                                                  "LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` "
                                                  ") data "
                                                  "LEFT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` "
                                                  "AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` "
                                                  "ORDER BY `m`, `ev1`, `ev2`");
        return ReadRows<BattleGroundEventIndexRow>(result, ReadBattleGroundEventIndex);
    }
}
