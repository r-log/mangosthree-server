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

#include "GameEventDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace GameEventDataRepository
{
    std::optional<uint16> QueryHighestEventEntry()
    {
        QueryResult* result = WorldDatabase.Query("SELECT MAX(`entry`) FROM `game_event`");
        return ReadRow<uint16>(result, [](Field* fields) { return fields[0].GetUInt16(); });
    }

    static GameEventRow ReadGameEvent(Field* fields)
    {
        GameEventRow row;
        row.entry = fields[0].GetUInt16();
        row.startTime = fields[1].GetUInt64();
        row.endTime = fields[2].GetUInt64();
        row.occurence = fields[3].GetUInt32();
        row.length = fields[4].GetUInt32();
        row.holiday = fields[5].GetUInt32();
        row.description = fields[6].GetCppString();
        return row;
    }

    std::vector<GameEventRow> QueryEvents()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,UNIX_TIMESTAMP(`start_time`),UNIX_TIMESTAMP(`end_time`),`occurence`,`length`,`holiday`,`description` FROM `game_event`");
        return ReadRows<GameEventRow>(result, ReadGameEvent);
    }

    static GameEventCreatureRow ReadGameEventCreature(Field* fields)
    {
        GameEventCreatureRow row;
        row.guid = fields[0].GetUInt32();
        row.event = fields[1].GetInt16();
        return row;
    }

    std::vector<GameEventCreatureRow> QueryEventCreatures()
    {
        //                                 1              2
        QueryResult* result = WorldDatabase.Query("SELECT `creature`.`guid`, `game_event_creature`.`event` "
                                                  "FROM `creature` JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid`");
        return ReadRows<GameEventCreatureRow>(result, ReadGameEventCreature);
    }

    static GameEventGameObjectRow ReadGameEventGameObject(Field* fields)
    {
        GameEventGameObjectRow row;
        row.guid = fields[0].GetUInt32();
        row.event = fields[1].GetInt16();
        return row;
    }

    std::vector<GameEventGameObjectRow> QueryEventGameObjects()
    {
        //                                 1                2
        QueryResult* result = WorldDatabase.Query("SELECT `gameobject`.`guid`, `game_event_gameobject`.`event` "
                                                  "FROM `gameobject` JOIN `game_event_gameobject` ON `gameobject`.`guid`=`game_event_gameobject`.`guid`");
        return ReadRows<GameEventGameObjectRow>(result, ReadGameEventGameObject);
    }

    static GameEventCreatureDataRow ReadGameEventCreatureData(Field* fields)
    {
        GameEventCreatureDataRow row;
        row.guid = fields[0].GetUInt32();
        row.event = fields[1].GetUInt16();
        row.modelid = fields[2].GetUInt32();
        row.equipmentId = fields[3].GetUInt32();
        row.entryId = fields[4].GetUInt32();
        row.spellStart = fields[5].GetUInt32();
        row.spellEnd = fields[6].GetUInt32();
        return row;
    }

    std::vector<GameEventCreatureDataRow> QueryEventCreatureData()
    {
        //                                 0              1                             2
        QueryResult* result = WorldDatabase.Query("SELECT `creature`.`guid`, `game_event_creature_data`.`event`, `game_event_creature_data`.`modelid`,"
                                                  //   3                                      4
                                                  "`game_event_creature_data`.`equipment_id`, `game_event_creature_data`.`entry_id`, "
                                                  //   5                                     6
                                                  "`game_event_creature_data`.`spell_start`, `game_event_creature_data`.`spell_end` "
                                                  "FROM `creature` JOIN `game_event_creature_data` ON `creature`.`guid`=`game_event_creature_data`.`guid`");
        return ReadRows<GameEventCreatureDataRow>(result, ReadGameEventCreatureData);
    }

    static GameEventQuestRow ReadGameEventQuest(Field* fields)
    {
        GameEventQuestRow row;
        row.quest = fields[0].GetUInt32();
        row.event = fields[1].GetUInt16();
        return row;
    }

    std::vector<GameEventQuestRow> QueryEventQuests()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `quest`, `event` FROM `game_event_quest`");
        return ReadRows<GameEventQuestRow>(result, ReadGameEventQuest);
    }

    static GameEventMailRow ReadGameEventMail(Field* fields)
    {
        GameEventMailRow row;
        row.event = fields[0].GetUInt16();
        row.raceMask = fields[1].GetUInt32();
        row.quest = fields[2].GetUInt32();
        row.mailTemplateId = fields[3].GetUInt32();
        row.senderEntry = fields[4].GetUInt32();
        return row;
    }

    std::vector<GameEventMailRow> QueryEventMails()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `event`, `raceMask`, `quest`, `mailTemplateId`, `senderEntry` FROM `game_event_mail`");
        return ReadRows<GameEventMailRow>(result, ReadGameEventMail);
    }
}
