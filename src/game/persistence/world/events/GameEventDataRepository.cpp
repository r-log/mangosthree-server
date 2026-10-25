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

namespace GameEventDataRepository
{
    QueryResult* QueryHighestEventEntry()
    {
        return WorldDatabase.Query("SELECT MAX(`entry`) FROM `game_event`");
    }

    QueryResult* QueryEvents()
    {
        return WorldDatabase.Query("SELECT `entry`,UNIX_TIMESTAMP(`start_time`),UNIX_TIMESTAMP(`end_time`),`occurence`,`length`,`holiday`,`description` FROM `game_event`");
    }

    QueryResult* QueryEventCreatures()
    {
        //                                 1              2
        return WorldDatabase.Query("SELECT `creature`.`guid`, `game_event_creature`.`event` "
                                   "FROM `creature` JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid`");
    }

    QueryResult* QueryEventGameObjects()
    {
        //                                 1                2
        return WorldDatabase.Query("SELECT `gameobject`.`guid`, `game_event_gameobject`.`event` "
                                   "FROM `gameobject` JOIN `game_event_gameobject` ON `gameobject`.`guid`=`game_event_gameobject`.`guid`");
    }

    QueryResult* QueryEventCreatureData()
    {
        //                                 0              1                             2
        return WorldDatabase.Query("SELECT `creature`.`guid`, `game_event_creature_data`.`event`, `game_event_creature_data`.`modelid`,"
                                   //   3                                      4
                                   "`game_event_creature_data`.`equipment_id`, `game_event_creature_data`.`entry_id`, "
                                   //   5                                     6
                                   "`game_event_creature_data`.`spell_start`, `game_event_creature_data`.`spell_end` "
                                   "FROM `creature` JOIN `game_event_creature_data` ON `creature`.`guid`=`game_event_creature_data`.`guid`");
    }

    QueryResult* QueryEventQuests()
    {
        return WorldDatabase.Query("SELECT `quest`, `event` FROM `game_event_quest`");
    }

    QueryResult* QueryEventMails()
    {
        return WorldDatabase.Query("SELECT `event`, `raceMask`, `quest`, `mailTemplateId`, `senderEntry` FROM `game_event_mail`");
    }
}
