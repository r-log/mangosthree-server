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

#include "WaypointRepository.h"
#include "Database/DatabaseEnv.h"

namespace WaypointRepository
{
    QueryResult* QueryPathPointCounts()
    {
        return WorldDatabase.Query("SELECT `id`, COUNT(`point`) FROM `creature_movement` GROUP BY `id`");
    }

    QueryResult* QueryPathPoints()
    {
        //                                 0   1      2           3           4           5         6
        return WorldDatabase.Query("SELECT `id`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,"
                                   //   7        8        9        10       11       12     13     14           15      16
                                   "`textid1`, `textid2`, `textid3`, `textid4`, `textid5`, `emote`, `spell`, `orientation`, `model1`, `model2` FROM `creature_movement`");
    }

    void FixPathPoint(float x, float y, float z, uint32 id, uint32 point)
    {
        WorldDatabase.PExecute("UPDATE `creature_movement` SET `position_x` = '%f', `position_y` = '%f', `position_z` = '%f' WHERE `id` = '%u' AND `point` = '%u'", x, y, z, id, point);
    }

    QueryResult* QueryTemplatePathPointCounts()
    {
        return WorldDatabase.Query("SELECT `entry`, COUNT(`point`) FROM `creature_movement_template` GROUP BY `entry`");
    }

    QueryResult* QueryTemplatePathPoints()
    {
        //                                 0      1      2           3           4           5         6
        return WorldDatabase.Query("SELECT `entry`, `point`, `position_x`, `position_y`, `position_z`, `waittime`, `script_id`,"
                                   //   7        8        9        10       11       12     13     14           15      16
                                   "`textid1`, `textid2`, `textid3`, `textid4`, `textid5`, `emote`, `spell`, `orientation`, `model1`, `model2` FROM `creature_movement_template`");
    }

    void FixTemplatePathPoint(float x, float y, uint32 entry, uint32 point)
    {
        WorldDatabase.PExecute("UPDATE `creature_movement_template` SET `position_x` = '%f', `position_y` = '%f' WHERE `entry` = %u AND `point` = %u", x, y, entry, point);
    }

    void ShiftPoint(char const* table, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("UPDATE `%s` SET `point`=`point`+1 WHERE `%s`=%u AND `point`=%u", table, keyField, key, point);
    }

    void InsertPoint(char const* table, char const* keyField, uint32 key, uint32 point, float x, float y, float z)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `%s` (`%s`,`point`,`position_x`,`position_y`,`position_z`,`orientation`) VALUES (%u,%u, %f,%f,%f, 100)", table, keyField, key, point, x, y, z);
    }

    void DeletePoint(char const* table, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("DELETE FROM `%s` WHERE `%s`=%u AND `point`=%u", table, keyField, key, point);
    }

    void DeletePath(uint32 id)
    {
        WorldDatabase.PExecuteLog("DELETE FROM `creature_movement` WHERE `id`=%u", id);
    }

    void SetPointPosition(char const* table, float x, float y, float z, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("UPDATE `%s` SET `position_x`=%f, `position_y`=%f, `position_z`=%f WHERE `%s`=%u AND `point`=%u", table, x, y, z, keyField, key, point);
    }

    void SetPointWaittime(char const* table, uint32 waittime, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("UPDATE `%s` SET `waittime`=%u WHERE `%s`=%u AND `point`=%u", table, waittime, keyField, key, point);
    }

    void SetPointOrientation(char const* table, float orientation, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("UPDATE `%s` SET `orientation`=%f WHERE `%s`=%u AND `point`=%u", table, orientation, keyField, key, point);
    }

    void SetPointScriptId(char const* table, uint32 scriptId, char const* keyField, uint32 key, uint32 point)
    {
        WorldDatabase.PExecuteLog("UPDATE `%s` SET `script_id`=%u WHERE `%s`=%u AND `point`=%u", table, scriptId, keyField, key, point);
    }
}
