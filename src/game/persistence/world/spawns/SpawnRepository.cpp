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

#include "SpawnRepository.h"
#include "Database/DatabaseEnv.h"

#include <sstream>

namespace SpawnRepository
{
    void ReplaceCreature(CreatureSpawnRow const& row)
    {
        WorldDatabase.BeginTransaction();

        WorldDatabase.PExecuteLog("DELETE FROM `creature` WHERE `guid`=%u", row.guid);

        std::ostringstream ss;
        ss << "INSERT INTO `creature` VALUES ("
           << row.guid << ","
           << row.id << ","
           << row.map << ","
           << row.spawnMask << ","
           << row.phaseMask << ","
           << row.modelIdOverride << ","
           << row.equipmentId << ","
           << row.x << ","
           << row.y << ","
           << row.z << ","
           << row.o << ","
           << row.spawnTimeSecs << ","                         // respawn time
           << row.spawnDist << ","                             // spawn distance (float)
           << row.currentWaypoint << ","                       // currentwaypoint
           << row.curHealth << ","                             // curhealth
           << row.curMana << ","                               // curmana
           << row.isDead << ","                                // is_dead
           << row.movementType << ")";                         // default movement generator type

        WorldDatabase.PExecuteLog("%s", ss.str().c_str());

        WorldDatabase.CommitTransaction();
    }

    void DeleteCreature(uint32 guid)
    {
        WorldDatabase.BeginTransaction();
        WorldDatabase.PExecuteLog("DELETE FROM `creature` WHERE `guid`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `creature_addon` WHERE `guid`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `creature_movement` WHERE `id`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `game_event_creature` WHERE `guid`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `game_event_creature_data` WHERE `guid`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `creature_battleground` WHERE `guid`=%u", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `creature_linking` WHERE `guid`=%u OR `master_guid`=%u", guid, guid);
        WorldDatabase.CommitTransaction();
    }

    void ReplaceGameObject(GameObjectSpawnRow const& row)
    {
        std::ostringstream ss;
        ss << "INSERT INTO `gameobject` VALUES ( "
           << row.guid << ", "
           << row.entry << ", "
           << row.map << ", "
           << row.spawnMask << ","
           << row.phaseMask << ","
           << row.x << ", "
           << row.y << ", "
           << row.z << ", "
           << row.o << ", "
           << row.rotationX << ", "
           << row.rotationY << ", "
           << row.rotationZ << ", "
           << row.rotationW << ", "
           << row.respawnDelay << ", "
           << row.animProgress << ", "
           << row.goState << ")";

        WorldDatabase.BeginTransaction();
        WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", row.guid);
        WorldDatabase.PExecuteLog("%s", ss.str().c_str());
        WorldDatabase.CommitTransaction();
    }

    void DeleteGameObject(uint32 guid)
    {
        WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `game_event_gameobject` WHERE `guid` = '%u'", guid);
        WorldDatabase.PExecuteLog("DELETE FROM `gameobject_battleground` WHERE `guid` = '%u'", guid);
    }

    QueryResult* QueryHighestCreatureGuid()
    {
        return WorldDatabase.Query("SELECT MAX(`guid`) FROM `creature`");
    }

    QueryResult* QueryHighestGameObjectGuid()
    {
        return WorldDatabase.Query("SELECT MAX(`guid`) FROM `gameobject`");
    }

    QueryResult* QueryCreatureSpawns()
    {
        //                                 0                       1   2    3
        return WorldDatabase.Query("SELECT `creature`.`guid`, `creature`.`id`, `map`, `modelid`,"
                                   //   4             5           6           7           8            9              10         11
                                   "`equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `spawndist`, `currentwaypoint`,"
                                   //   12        13         14            15              16           17           18
                                   "`curhealth`, `curmana`, `DeathState`, `MovementType`, `spawnMask`, `phaseMask`, `event`,"
                                   //                19                                     20
                                   "`pool_creature`.`pool_entry`, `pool_creature_template`.`pool_entry` "
                                   "FROM `creature` "
                                   "LEFT OUTER JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid` "
                                   "LEFT OUTER JOIN `pool_creature` ON `creature`.`guid` = `pool_creature`.`guid` "
                                   "LEFT OUTER JOIN `pool_creature_template` ON `creature`.`id` = `pool_creature_template`.`id`");
    }

    QueryResult* QueryGameObjectSpawns()
    {
        //                                               0                    1                  2                   3
        return WorldDatabase.Query("SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, "
            //                                   4                          5                          6                           7
               "`gameobject`.`position_y`, `gameobject`.`position_z`, `gameobject`.`orientation`, `gameobject`.`rotation0`, "
            //                                   8                         9                         10                        11
               "`gameobject`.`rotation1`, `gameobject`.`rotation2`, `gameobject`.`rotation3`, `gameobject`.`spawntimesecs`, "
            //                                   12                           13                    14                        15
               "`gameobject`.`animprogress`, `gameobject`.`state`, `gameobject`.`spawnMask`, `gameobject`.`phaseMask`, "
            //                                             16                          17                                       18
               "`game_event_gameobject`.`event`, `pool_gameobject`.`pool_entry`, `pool_gameobject_template`.`pool_entry` "
               "FROM `gameobject` "
               "LEFT OUTER JOIN `game_event_gameobject` ON `gameobject`.`guid` = `game_event_gameobject`.`guid` "
               "LEFT OUTER JOIN `pool_gameobject` ON `gameobject`.`guid` = `pool_gameobject`.`guid` "
               "LEFT OUTER JOIN `pool_gameobject_template` ON `gameobject`.`id` = `pool_gameobject_template`.`id`");
    }

    QueryResult* QueryCreatureEntryAndMap(uint32 guid)
    {
        return WorldDatabase.PQuery("SELECT `id`, `map` FROM `creature` WHERE `guid` = '%u'", guid);
    }

    QueryResult* QueryCreatureGuidsOnMap(uint32 entry, uint32 mapId)
    {
        return WorldDatabase.PQuery("SELECT `guid` FROM `creature` WHERE `id`=%u AND `map`=%u LIMIT 2", entry, mapId);
    }

    QueryResult* QueryGameObjectCountOf(uint32 entry)
    {
        return WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='%u'", entry);
    }

    QueryResult* QueryGameObjectsNear(float x, float y, float z, uint32 entry, uint32 count)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                    x, y, z, entry, count);
    }

    QueryResult* QueryGameObjectsOf(uint32 entry, uint32 count)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `gameobject` WHERE `id` = '%u' LIMIT %u",
                                    entry, count);
    }

    QueryResult* QueryCreatureCountOf(uint32 entry)
    {
        return WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `creature` WHERE `id`='%u'", entry);
    }

    QueryResult* QueryCreaturesNear(float x, float y, float z, uint32 entry, uint32 count)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `creature` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                    x, y, z, entry, count);
    }

    QueryResult* QueryCreaturesOf(uint32 entry, uint32 count)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `creature` WHERE `id` = '%u' LIMIT %u",
                                    entry, count);
    }
}
