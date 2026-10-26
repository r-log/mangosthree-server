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
#include "persistence/RowReader.h"

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

    std::optional<uint32> QueryHighestCreatureGuid()
    {
        QueryResult* result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `creature`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint32> QueryHighestGameObjectGuid()
    {
        QueryResult* result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `gameobject`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static CreatureSpawnDataRow ReadCreatureSpawnData(Field* fields)
    {
        CreatureSpawnDataRow row;
        row.guid = fields[0].GetUInt32();
        row.id = fields[1].GetUInt32();
        row.map = fields[2].GetUInt32();
        row.modelid = fields[3].GetUInt32();
        row.equipmentId = fields[4].GetUInt32();
        row.positionX = fields[5].GetFloat();
        row.positionY = fields[6].GetFloat();
        row.positionZ = fields[7].GetFloat();
        row.orientation = fields[8].GetFloat();
        row.spawntimesecs = fields[9].GetUInt32();
        row.spawndist = fields[10].GetFloat();
        row.currentwaypoint = fields[11].GetUInt32();
        row.curhealth = fields[12].GetUInt32();
        row.curmana = fields[13].GetUInt32();
        row.deathState = fields[14].GetBool();
        row.movementType = fields[15].GetUInt8();
        row.spawnMask = fields[16].GetUInt8();
        row.phaseMask = fields[17].GetUInt32();
        row.gameEvent = fields[18].GetInt16();
        row.guidPoolId = fields[19].GetInt16();
        row.entryPoolId = fields[20].GetInt16();
        return row;
    }

    std::vector<CreatureSpawnDataRow> QueryCreatureSpawns()
    {
        //                                 0                       1   2    3
        QueryResult* result = WorldDatabase.Query("SELECT `creature`.`guid`, `creature`.`id`, `map`, `modelid`,"
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
        return ReadRows<CreatureSpawnDataRow>(result, ReadCreatureSpawnData);
    }

    static GameObjectSpawnDataRow ReadGameObjectSpawnData(Field* fields)
    {
        GameObjectSpawnDataRow row;
        row.guid = fields[0].GetUInt32();
        row.id = fields[1].GetUInt32();
        row.map = fields[2].GetUInt32();
        row.positionX = fields[3].GetFloat();
        row.positionY = fields[4].GetFloat();
        row.positionZ = fields[5].GetFloat();
        row.orientation = fields[6].GetFloat();
        row.rotation0 = fields[7].GetFloat();
        row.rotation1 = fields[8].GetFloat();
        row.rotation2 = fields[9].GetFloat();
        row.rotation3 = fields[10].GetFloat();
        row.spawntimesecs = fields[11].GetInt32();
        row.animprogress = fields[12].GetUInt32();
        row.state = fields[13].GetUInt32();
        row.spawnMask = fields[14].GetUInt8();
        row.phaseMask = fields[15].GetUInt32();
        row.gameEvent = fields[16].GetInt16();
        row.guidPoolId = fields[17].GetInt16();
        row.entryPoolId = fields[18].GetInt16();
        return row;
    }

    std::vector<GameObjectSpawnDataRow> QueryGameObjectSpawns()
    {
        //                                               0                    1                  2                   3
        QueryResult* result = WorldDatabase.Query("SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, "
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
        return ReadRows<GameObjectSpawnDataRow>(result, ReadGameObjectSpawnData);
    }

    static CreatureEntryAndMapRow ReadCreatureEntryAndMap(Field* fields)
    {
        CreatureEntryAndMapRow row;
        row.id = fields[0].GetUInt32();
        row.map = fields[1].GetUInt32();
        return row;
    }

    std::optional<CreatureEntryAndMapRow> QueryCreatureEntryAndMap(uint32 guid)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `id`, `map` FROM `creature` WHERE `guid` = '%u'", guid);
        return ReadRow<CreatureEntryAndMapRow>(result, ReadCreatureEntryAndMap);
    }

    std::vector<uint32> QueryCreatureGuidsOnMap(uint32 entry, uint32 mapId)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `creature` WHERE `id`=%u AND `map`=%u LIMIT 2", entry, mapId);
        return ReadRows<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::optional<uint32> QueryGameObjectCountOf(uint32 entry)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='%u'", entry);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static SpawnListRow ReadSpawnListRow(Field* fields)
    {
        SpawnListRow row;
        row.guid = fields[0].GetUInt32();
        row.positionX = fields[1].GetFloat();
        row.positionY = fields[2].GetFloat();
        row.positionZ = fields[3].GetFloat();
        row.map = fields[4].GetUInt16();
        return row;
    }

    std::vector<SpawnListRow> QueryGameObjectsNear(float x, float y, float z, uint32 entry, uint32 count)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                                   x, y, z, entry, count);
        return ReadRows<SpawnListRow>(result, ReadSpawnListRow);
    }

    std::vector<SpawnListRow> QueryGameObjectsOf(uint32 entry, uint32 count)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `gameobject` WHERE `id` = '%u' LIMIT %u",
                                                   entry, count);
        return ReadRows<SpawnListRow>(result, ReadSpawnListRow);
    }

    std::optional<uint32> QueryCreatureCountOf(uint32 entry)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `creature` WHERE `id`='%u'", entry);
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    std::vector<SpawnListRow> QueryCreaturesNear(float x, float y, float z, uint32 entry, uint32 count)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `creature` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                                   x, y, z, entry, count);
        return ReadRows<SpawnListRow>(result, ReadSpawnListRow);
    }

    std::vector<SpawnListRow> QueryCreaturesOf(uint32 entry, uint32 count)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `creature` WHERE `id` = '%u' LIMIT %u",
                                                   entry, count);
        return ReadRows<SpawnListRow>(result, ReadSpawnListRow);
    }

    void SetCreaturePosition(float x, float y, float z, float o, uint32 guid)
    {
        WorldDatabase.PExecuteLog("UPDATE `creature` SET `position_x` = '%f', `position_y` = '%f', `position_z` = '%f', `orientation` = '%f' WHERE `guid` = '%u'", x, y, z, o, guid);
    }

    void SetCreatureSpawnDistance(float distance, int32 movementType, uint32 guid)
    {
        WorldDatabase.PExecuteLog("UPDATE `creature` SET `spawndist`=%f, `MovementType`=%i WHERE `guid`=%u", distance, movementType, guid);
    }

    void SetCreatureSpawnTime(uint32 seconds, uint32 guid)
    {
        WorldDatabase.PExecuteLog("UPDATE `creature` SET `spawntimesecs`=%i WHERE `guid`=%u", seconds, guid);
    }

    QueryResult* QueryGameObjectsInRange(float x, float y, float z, uint32 mapId, float distanceSquared)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `map`, "
                                    "(POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ "
                                    "FROM `gameobject` WHERE `map`='%u' AND (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) <= '%f' ORDER BY order_",
                                    x, y, z,
                                    mapId, x, y, z, distanceSquared);
    }

    QueryResult* QueryNearestGameObjectOf(float x, float y, float z, uint32 mapId, uint32 entry)
    {
        return WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `map` = '%i' AND `id` = '%u' ORDER BY order_ ASC LIMIT 1",
                                    x, y, z, mapId, entry);
    }

    QueryResult* QueryNearestGameObjectNamed(float x, float y, float z, uint32 mapId, std::string name)
    {
        WorldDatabase.escape_string(name);
        return WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) AS order_ "
                                    "FROM `gameobject`,`gameobject_template` WHERE `gameobject_template`.`entry` = `gameobject`.`id` AND `map` = %i AND `name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'")" ORDER BY order_ ASC LIMIT 1",
                                    x, y, z, mapId, name.c_str());
    }

    QueryResult* QueryNearestGameObjects(float x, float y, float z, uint32 mapId, std::set<uint16> const& activeEvents)
    {
        std::ostringstream eventFilter;
        eventFilter << " AND (event IS NULL ";
        bool initString = true;

        for (std::set<uint16>::const_iterator itr = activeEvents.begin(); itr != activeEvents.end(); ++itr)
        {
            if (initString)
            {
                eventFilter << "OR event IN (" << *itr;
                initString = false;
            }
            else
            {
                eventFilter << "," << *itr;
            }
        }

        if (!initString)
        {
            eventFilter << "))";
        }
        else
        {
            eventFilter << ")";
        }

        return WorldDatabase.PQuery("SELECT `gameobject`.`guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, "
                                    "(POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) AS order_ FROM `gameobject` "
                                    "LEFT OUTER JOIN `game_event_gameobject` on `gameobject`.`guid`=`game_event_gameobject`.`guid` WHERE `map` = '%i' %s ORDER BY order_ ASC LIMIT 10",
                                    x, y, z, mapId, eventFilter.str().c_str());
    }

    QueryResult* QueryCreatureGuidsNamed(std::string name)
    {
        WorldDatabase.escape_string(name);
        return WorldDatabase.PQuery("SELECT `guid` FROM `creature`, `creature_template` WHERE `creature`.`id` = `creature_template`.`entry` AND `creature_template`.`name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
    }

    QueryResult* QueryGameObjectGuidsNamed(std::string name)
    {
        WorldDatabase.escape_string(name);
        return WorldDatabase.PQuery("SELECT `guid` FROM `gameobject`, `gameobject_template` WHERE `gameobject`.`id` = `gameobject_template`.`entry` AND `gameobject_template`.`name `" _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
    }

    QueryResult* QueryCreatureIdCountOf(uint32 entry)
    {
        return WorldDatabase.PQuery("SELECT COUNT(`id`) FROM `creature` WHERE `id` = %u", entry);
    }
}
