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
}
