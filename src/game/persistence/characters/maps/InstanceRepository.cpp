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

#include "InstanceRepository.h"
#include "Database/DatabaseEnv.h"
#include "Utilities/Errors.h"
#include "Utilities/Util.h"

#include <cstdarg>
#include <cstdio>
#include <sstream>

namespace
{
    /// Selects the rows a query tail names and deletes them one by one, each by the values of the
    /// fields listed, escaped.
    void DeleteSelected(const char* fields, const char* table, const char* queryTail, ...)
    {
        Tokens fieldTokens = StrSplit(fields, ", ");
        MANGOS_ASSERT(fieldTokens.size() != 0);

        va_list ap;
        char szQueryTail [MAX_QUERY_LEN];
        va_start(ap, queryTail);
        vsnprintf(szQueryTail, MAX_QUERY_LEN, queryTail, ap);
        va_end(ap);

        // query is delimited in input
        QueryResult* result = CharacterDatabase.PQuery("SELECT %s FROM %s %s", fields, table, szQueryTail);
        if (result)
        {
            do
            {
                Field* row = result->Fetch();
                std::ostringstream ss;
                for (size_t i = 0; i < fieldTokens.size(); ++i)
                {
                    std::string fieldValue = row[i].GetCppString();
                    CharacterDatabase.escape_string(fieldValue);
                    ss << (i != 0 ? " AND " : "") << fieldTokens[i] << " = '" << fieldValue << "'";
                }
                CharacterDatabase.PExecute("DELETE FROM %s WHERE %s", table, ss.str().c_str());
            }
            while (result->NextRow());
            delete result;
        }
    }
}

namespace InstanceRepository
{
    void SaveCreatureRespawn(uint32 guid, uint32 instanceId, uint64 respawnTime, bool keep)
    {
        CharacterDatabase.BeginTransaction();

        static SqlStatementID delSpawnTime ;
        static SqlStatementID insSpawnTime ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `creature_respawn` WHERE `guid` = ? AND `instance` = ?");
        stmt.PExecute(guid, instanceId);

        if (keep)
        {
            stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `creature_respawn` VALUES ( ?, ?, ? )");
            stmt.PExecute(guid, respawnTime, instanceId);
        }

        CharacterDatabase.CommitTransaction();
    }

    void SaveGameObjectRespawn(uint32 guid, uint32 instanceId, uint64 respawnTime, bool keep)
    {
        CharacterDatabase.BeginTransaction();

        static SqlStatementID delSpawnTime ;
        static SqlStatementID insSpawnTime ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delSpawnTime, "DELETE FROM `gameobject_respawn` WHERE `guid` = ? AND `instance` = ?");
        stmt.PExecute(guid, instanceId);

        if (keep)
        {
            stmt = CharacterDatabase.CreateStatement(insSpawnTime, "INSERT INTO `gameobject_respawn` VALUES ( ?, ?, ? )");
            stmt.PExecute(guid, respawnTime, instanceId);
        }

        CharacterDatabase.CommitTransaction();
    }

    void DeleteRespawnsOfInstance(uint32 instanceId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.CommitTransaction();
    }

    void DeleteExpiredCreatureRespawnsNow()
    {
        CharacterDatabase.DirectExecute("DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");
    }

    void DeleteExpiredGameObjectRespawnsNow()
    {
        CharacterDatabase.DirectExecute("DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())");
    }

    QueryResult* QueryCreatureRespawns()
    {
        //                                    0       1              2      3           4             5            6
        return CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`");
    }

    QueryResult* QueryGameObjectRespawns()
    {
        //                                    0       1              2      3           4             5            6
        return CharacterDatabase.Query("SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`");
    }

    void InsertInstance(uint32 instanceId, uint32 map, uint64 resetTime, uint32 difficulty, uint32 encountersMask, std::string const& data)
    {
        static SqlStatementID insInstance;
        SqlStatement insert = CharacterDatabase.CreateStatement(insInstance,
                              "INSERT INTO `instance` (`id`, `map`, `resettime`, `difficulty`, `encountersMask`, `data`) "
                              "VALUES (?, ?, ?, ?, ?, ?)");
        insert.addUInt32(instanceId);
        insert.addUInt32(map);
        insert.addUInt64(resetTime);
        insert.addUInt32(difficulty);
        insert.addUInt32(encountersMask);
        insert.addString(data);
        insert.Execute();
    }

    void SetEncountersMask(uint32 encountersMask, uint32 instanceId)
    {
        CharacterDatabase.PExecute("UPDATE `instance` SET `encountersMask` = '%u' WHERE `id` = '%u'", encountersMask, instanceId);
    }

    void SetResetTime(uint64 resetTime, uint32 instanceId)
    {
        CharacterDatabase.PExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", resetTime, instanceId);
    }

    void SetResetTimeNow(uint64 resetTime, uint32 instanceId)
    {
        CharacterDatabase.DirectPExecute("UPDATE `instance` SET `resettime` = '" UI64FMTD "' WHERE `id` = '%u'", resetTime, instanceId);
    }

    void DeleteInstance(uint32 instanceId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `id` = '%u'", instanceId);
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.PExecute("DELETE FROM `creature_respawn` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.PExecute("DELETE FROM `gameobject_respawn` WHERE `instance` = '%u'", instanceId);
        CharacterDatabase.CommitTransaction();
    }

    void DeleteInstancesOfMap(uint32 map)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_instance` USING `character_instance` LEFT JOIN `instance` ON `character_instance`.`instance` = `id` WHERE `map` = '%u'", map);
        CharacterDatabase.PExecute("DELETE FROM `group_instance` USING `group_instance` LEFT JOIN `instance` ON `group_instance`.`instance` = `id` WHERE `map` = '%u'", map);
        CharacterDatabase.PExecute("DELETE FROM `instance` WHERE `map` = '%u'", map);
        CharacterDatabase.CommitTransaction();
    }

    QueryResult* QueryInstancesWithResetTime()
    {
        return CharacterDatabase.Query("SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0");
    }

    QueryResult* QueryLatestCreatureRespawns()
    {
        return CharacterDatabase.Query("SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`");
    }

    QueryResult* QueryInstanceIds()
    {
        return CharacterDatabase.Query("SELECT `id` FROM `instance`");
    }

    void RenumberInstance(uint32 newId, uint32 oldId)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("UPDATE `creature_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `gameobject_respawn` SET `instance` = '%u' WHERE `instance` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `corpse` SET `instance` = '%u' WHERE `instance` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u' WHERE `instance` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `instance` SET `id` = '%u' WHERE `id` = '%u'", newId, oldId);
        CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u' WHERE `instance` = '%u'", newId, oldId);
        CharacterDatabase.CommitTransaction();
    }

    QueryResult* QueryInstanceResets()
    {
        return CharacterDatabase.Query("SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`");
    }

    void SetInstanceReset(uint64 resetTime, uint32 map, uint32 difficulty)
    {
        CharacterDatabase.PExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u' AND `difficulty` = '%u'", resetTime, map, difficulty);
    }

    void SetInstanceResetNow(uint64 resetTime, uint32 map, uint32 difficulty)
    {
        CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE `mapid` = '%u' AND `difficulty` = '%u'", resetTime, map, difficulty);
    }

    void SetInstanceResetNowUnquoted(uint64 resetTime, uint32 map, uint32 difficulty)
    {
        CharacterDatabase.DirectPExecute("UPDATE `instance_reset` SET `resettime` = '" UI64FMTD "' WHERE mapid = '%u' AND difficulty= '%u'", resetTime, map, difficulty);
    }

    void InsertInstanceResetNow(uint32 map, uint32 difficulty, uint64 resetTime)
    {
        CharacterDatabase.DirectPExecute("INSERT INTO `instance_reset` VALUES ('%u','%u','" UI64FMTD "')", map, difficulty, resetTime);
    }

    void DeleteInstanceResetNow(uint32 map, uint32 difficulty)
    {
        CharacterDatabase.DirectPExecute("DELETE FROM `instance_reset` WHERE `mapid` = '%u' AND `difficulty` = '%u'", map, difficulty);
    }

    void DeleteCharacterBindsOfMissingCharacters()
    {
        DeleteSelected("`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `characters` ON `character_instance`.`guid` = `characters`.`guid` WHERE `characters`.`guid` IS NULL");
    }

    void DeleteGroupBindsOfMissingGroups()
    {
        DeleteSelected("`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `characters` ON `group_instance`.`leaderGuid` = `characters`.`guid` LEFT JOIN `groups` ON `group_instance`.`leaderGuid` = `groups`.`leaderGuid` WHERE `characters`.`guid` IS NULL OR `groups`.`leaderGuid` IS NULL");
    }

    void DeleteUnboundInstances()
    {
        DeleteSelected("`id`, `map`, `difficulty`", "`instance`", "LEFT JOIN `character_instance` ON `character_instance`.`instance` = `id` LEFT JOIN `group_instance` ON `group_instance`.`instance` = `id` WHERE `character_instance`.`instance` IS NULL AND `group_instance`.`instance` IS NULL");
    }

    void DeleteCharacterBindsOfMissingInstances()
    {
        DeleteSelected("`character_instance`.`guid`, `instance`", "`character_instance`", "LEFT JOIN `instance` ON `character_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");
    }

    void DeleteGroupBindsOfMissingInstances()
    {
        DeleteSelected("`group_instance`.`leaderGuid`, `instance`", "`group_instance`", "LEFT JOIN `instance` ON `group_instance`.`instance` = `instance`.`id` WHERE `instance`.`id` IS NULL");
    }

    void DeleteRespawnsOfMissingInstances()
    {
        CharacterDatabase.Execute("DELETE FROM `creature_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");
        CharacterDatabase.Execute("DELETE FROM `gameobject_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)");
    }

    void DeleteExpiredInstances(uint64 time)
    {
        DeleteSelected("id, map, instance.difficulty", "instance", "LEFT JOIN instance_reset ON mapid = map AND instance.difficulty =  instance_reset.difficulty WHERE (instance.resettime < '" UI64FMTD "' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS NULL AND instance_reset.resettime < '" UI64FMTD "')", time, time);
    }

    QueryResult* QueryHighestId()
    {
        return CharacterDatabase.Query("SELECT MAX(`id`) FROM `instance`");
    }
}
