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

/// The instances' repository on the global CharacterDatabase backed by fakes: the queued writes in
/// order with their values substituted; the startup reads on the query connection and the direct
/// writes on the async connection; the cleanup's select-then-delete driven by a canned answer.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/maps/InstanceRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on the global character database, asynchronous writes on.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
            return async.executed;
        }
    };

    void CheckLines(std::vector<std::string> const& got, std::vector<std::string> const& want, int line)
    {
        if (got == want)
        {
            return;
        }
        std::string text = "lines differ (case line " + std::to_string(line) + "): got [";
        for (size_t i = 0; i < got.size(); ++i)
        {
            text += (i ? " | " : "") + got[i];
        }
        text += "] want [";
        for (size_t i = 0; i < want.size(); ++i)
        {
            text += (i ? " | " : "") + want[i];
        }
        testing::ReportFailure(__FILE__, line, text + "]");
    }
}

// The queued writes, each respawn save and each multi-table delete one transaction.
TEST(InstanceRepository_Writes)
{
    Db db;
    {
        TickGuard::Scope scope;
        InstanceRepository::SaveCreatureRespawn(10, 3, 1700000000, true);
        InstanceRepository::SaveGameObjectRespawn(11, 3, 1700000000, false);
        InstanceRepository::DeleteRespawnsOfInstance(3);
        InstanceRepository::InsertInstance(3, 33, 1700000100, 1, 5, "a b c");
        InstanceRepository::SetEncountersMask(7, 3);
        InstanceRepository::SetResetTime(1700000200, 3);
        InstanceRepository::DeleteInstance(4);
        InstanceRepository::DeleteInstancesOfMap(36);
        InstanceRepository::RenumberInstance(1, 9);
        InstanceRepository::SetInstanceReset(1700000300, 33, 2);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(10));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `creature_respawn` WHERE `guid` = '10' AND `instance` = '3'",
        "INSERT INTO `creature_respawn` VALUES ( '10', '1700000000', '3' )",
        "DELETE FROM `gameobject_respawn` WHERE `guid` = '11' AND `instance` = '3'",
        "DELETE FROM `creature_respawn` WHERE `instance` = '3'",
        "DELETE FROM `gameobject_respawn` WHERE `instance` = '3'",
        "INSERT INTO `instance` (`id`, `map`, `resettime`, `difficulty`, `encountersMask`, `data`) VALUES ('3', '33', '1700000100', '1', '5', 'a b c')",
        "UPDATE `instance` SET `encountersMask` = '7' WHERE `id` = '3'",
        "UPDATE `instance` SET `resettime` = '1700000200' WHERE `id` = '3'",
        "DELETE FROM `instance` WHERE `id` = '4'",
        "DELETE FROM `character_instance` WHERE `instance` = '4'",
        "DELETE FROM `group_instance` WHERE `instance` = '4'",
        "DELETE FROM `creature_respawn` WHERE `instance` = '4'",
        "DELETE FROM `gameobject_respawn` WHERE `instance` = '4'",
        "DELETE FROM `character_instance` USING `character_instance` LEFT JOIN `instance` ON `character_instance`.`instance` = `id` WHERE `map` = '36'",
        "DELETE FROM `group_instance` USING `group_instance` LEFT JOIN `instance` ON `group_instance`.`instance` = `id` WHERE `map` = '36'",
        "DELETE FROM `instance` WHERE `map` = '36'",
        "UPDATE `creature_respawn` SET `instance` = '1' WHERE `instance` = '9'",
        "UPDATE `gameobject_respawn` SET `instance` = '1' WHERE `instance` = '9'",
        "UPDATE `corpse` SET `instance` = '1' WHERE `instance` = '9'",
        "UPDATE `character_instance` SET `instance` = '1' WHERE `instance` = '9'",
        "UPDATE `instance` SET `id` = '1' WHERE `id` = '9'",
        "UPDATE `group_instance` SET `instance` = '1' WHERE `instance` = '9'",
        "UPDATE `instance_reset` SET `resettime` = '1700000300' WHERE `mapid` = '33' AND `difficulty` = '2'",
    }, __LINE__);
}

// The startup reads run on the query connection and the direct writes on the async connection,
// both on the calling thread, each in call order; nothing is queued.
TEST(InstanceRepository_StartupReadsAndDirectWrites)
{
    Db db;
    InstanceRepository::DeleteExpiredCreatureRespawnsNow();
    delete InstanceRepository::QueryCreatureRespawns();
    InstanceRepository::DeleteExpiredGameObjectRespawnsNow();
    delete InstanceRepository::QueryGameObjectRespawns();
    delete InstanceRepository::QueryInstancesWithResetTime();
    delete InstanceRepository::QueryLatestCreatureRespawns();
    InstanceRepository::SetResetTimeNow(1700000400, 5);
    delete InstanceRepository::QueryInstanceResets();
    InstanceRepository::DeleteInstanceResetNow(33, 9);
    InstanceRepository::SetInstanceResetNow(1700000500, 33, 1);
    InstanceRepository::InsertInstanceResetNow(34, 1, 1700000600);
    InstanceRepository::SetInstanceResetNowUnquoted(1700000700, 34, 1);
    delete InstanceRepository::QueryInstanceIds();
    CheckLines(db.query.executed, {
        "SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `creature_respawn` LEFT JOIN `instance` ON `instance` = `id`",
        "SELECT `guid`, `respawntime`, `map`, `instance`, `difficulty`, `resettime`, `encountersMask` FROM `gameobject_respawn` LEFT JOIN `instance` ON `instance` = `id`",
        "SELECT `id`, `map`, `difficulty`, `resettime` FROM `instance` WHERE `resettime` > 0",
        "SELECT MAX(`respawntime`), `instance` FROM `creature_respawn` WHERE `instance` > 0 GROUP BY `instance`",
        "SELECT `mapid`, `difficulty`, `resettime` FROM `instance_reset`",
        "SELECT `id` FROM `instance`",
    }, __LINE__);
    CheckLines(db.async.executed, {
        "DELETE FROM `creature_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())",
        "DELETE FROM `gameobject_respawn` WHERE `respawntime` <= UNIX_TIMESTAMP(NOW())",
        "UPDATE `instance` SET `resettime` = '1700000400' WHERE `id` = '5'",
        "DELETE FROM `instance_reset` WHERE `mapid` = '33' AND `difficulty` = '9'",
        "UPDATE `instance_reset` SET `resettime` = '1700000500' WHERE `mapid` = '33' AND `difficulty` = '1'",
        "INSERT INTO `instance_reset` VALUES ('34','1','1700000600')",
        "UPDATE `instance_reset` SET `resettime` = '1700000700' WHERE mapid = '34' AND difficulty= '1'",
    }, __LINE__);
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(0));
}

// The cleanup selects the rows it removes on the calling thread, then queues one DELETE per row,
// each by the selected fields' values; its writes join the transaction the cleanup opens.
TEST(InstanceRepository_CleanupDeletesWhatItSelects)
{
    Db db;
    db.query.Answer("SELECT `character_instance`.`guid`, `instance` FROM `character_instance` LEFT JOIN `characters`",
                    FakeRows{FakeRow{"42", "7"}, FakeRow{"43", "8"}});
    db.query.Answer("SELECT id, map, instance.difficulty FROM instance LEFT JOIN instance_reset",
                    FakeRows{FakeRow{"9", "33", "1"}});
    CharacterTransaction::Begin();
    InstanceRepository::DeleteCharacterBindsOfMissingCharacters();
    InstanceRepository::DeleteRespawnsOfMissingInstances();
    CharacterTransaction::Commit();
    InstanceRepository::DeleteExpiredInstances(1700000800);
    REQUIRE(db.query.executed.size() == size_t(2));
    CHECK_STR(db.query.executed[0], "SELECT `character_instance`.`guid`, `instance` FROM `character_instance` LEFT JOIN `characters` ON `character_instance`.`guid` = `characters`.`guid` WHERE `characters`.`guid` IS NULL");
    CHECK_STR(db.query.executed[1], "SELECT id, map, instance.difficulty FROM instance LEFT JOIN instance_reset ON mapid = map AND instance.difficulty =  instance_reset.difficulty WHERE (instance.resettime < '1700000800' AND instance.resettime > '0') OR (NOT instance_reset.resettime IS NULL AND instance_reset.resettime < '1700000800')");
    CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(2));
    CheckLines(db.Take(), {
        "DELETE FROM `character_instance` WHERE `character_instance`.`guid` = '42' AND `instance` = '7'",
        "DELETE FROM `character_instance` WHERE `character_instance`.`guid` = '43' AND `instance` = '8'",
        "DELETE FROM `creature_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)",
        "DELETE FROM `gameobject_respawn` WHERE `instance` <> 0 AND `instance` NOT IN (SELECT `id` FROM `instance`)",
        "DELETE FROM instance WHERE id = '9' AND map = '33' AND instance.difficulty = '1'",
    }, __LINE__);
}
