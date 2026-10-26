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

/// The spawn repository on the global WorldDatabase and the corpse repository on the global
/// CharacterDatabase, backed by fakes: every write queued in order, the INSERTs' values written as
/// the stream prints them.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/CharacterTransaction.h"
#include "persistence/characters/maps/CorpseRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

#include <string>
#include <vector>

namespace
{
    /// The fakes on one of the global databases, asynchronous writes on.
    struct Db
    {
        Database& database;
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        explicit Db(Database& db) : database(db), query(db), async(db),
            attached(db, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            database.ExecuteQueuedForTest();
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

// A creature's and a game object's save are each one transaction (the old row's DELETE, then the
// INSERT); a creature's delete is one transaction of seven DELETEs, a game object's three DELETEs.
TEST(SpawnRepository_Writes)
{
    Db db(WorldDatabase);
    CreatureSpawnRow creature;
    creature.guid = 42;
    creature.id = 1234;
    creature.map = 0;
    creature.spawnMask = 1;
    creature.phaseMask = 1;
    creature.equipmentId = -1;
    creature.x = 1.5f;
    creature.y = -2.25f;
    creature.z = 3.0f;
    creature.o = 0.5f;
    creature.spawnTimeSecs = 300;
    creature.spawnDist = 5.0f;
    creature.curHealth = 100;
    creature.isDead = 1;
    creature.movementType = 1;
    GameObjectSpawnRow go;
    go.guid = 43;
    go.entry = 5678;
    go.map = 1;
    go.spawnMask = 1;
    go.phaseMask = 1;
    go.x = 10.5f;
    go.rotationW = 1.0f;
    go.respawnDelay = 60;
    go.animProgress = 100;
    go.goState = 1;
    {
        TickGuard::Scope scope;
        SpawnRepository::ReplaceCreature(creature);
        SpawnRepository::DeleteCreature(42);
        SpawnRepository::ReplaceGameObject(go);
        SpawnRepository::DeleteGameObject(43);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(WorldDatabase.GetDelayQueueDepth(), size_t(6));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `creature` WHERE `guid`=42",
        "INSERT INTO `creature` VALUES (42,1234,0,1,1,0,-1,1.5,-2.25,3,0.5,300,5,0,100,0,1,1)",
        "DELETE FROM `creature` WHERE `guid`=42",
        "DELETE FROM `creature_addon` WHERE `guid`=42",
        "DELETE FROM `creature_movement` WHERE `id`=42",
        "DELETE FROM `game_event_creature` WHERE `guid`=42",
        "DELETE FROM `game_event_creature_data` WHERE `guid`=42",
        "DELETE FROM `creature_battleground` WHERE `guid`=42",
        "DELETE FROM `creature_linking` WHERE `guid`=42 OR `master_guid`=42",
        "DELETE FROM `gameobject` WHERE `guid` = '43'",
        "INSERT INTO `gameobject` VALUES ( 43, 5678, 1, 1,1,10.5, 0, 0, 0, 0, 0, 0, 1, 60, 100, 1)",
        "DELETE FROM `gameobject` WHERE `guid` = '43'",
        "DELETE FROM `game_event_gameobject` WHERE `guid` = '43'",
        "DELETE FROM `gameobject_battleground` WHERE `guid` = '43'",
    }, __LINE__);
}

// A corpse's save joins the transaction the corpse opens: the player's old corpses' DELETE, then
// the INSERT.
TEST(SpawnRepository_Corpse)
{
    Db db(CharacterDatabase);
    CorpseRow row;
    row.guid = 7;
    row.player = 42;
    row.x = 1.5f;
    row.y = 2.0f;
    row.z = -3.5f;
    row.o = 0.25f;
    row.map = 530;
    row.time = 1700000000;
    row.type = 1;
    row.instance = 0;
    row.phaseMask = 1;
    {
        TickGuard::Scope scope;
        CharacterTransaction::Begin();
        CorpseRepository::DeleteOfPlayer(42);
        CorpseRepository::Insert(row);
        CharacterTransaction::Commit();
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(1));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `corpse` WHERE `player` = '42' AND `corpse_type` <> '0'",
        "INSERT INTO `corpse` (`guid`,`player`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`time`,`corpse_type`,`instance`,`phaseMask`) "
        "VALUES (7, 42, 1.5, 2, -3.5, 0.25, 530, 1700000000, 1, 0, 1)",
    }, __LINE__);
}

// Each column of a creature spawn as the startup load reads it lands in its member, and the rows
// come back in order.
TEST(SpawnRepository_CreatureSpawns)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005.5", "1006.5", "1007.5", "1008.5",
            "1009", "1010.5", "1011", "1012", "1013", "1", "115", "116", "1017", "-1018",
            "-1019", "-1020"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005.5", "2006.5", "2007.5", "2008.5",
            "2009", "2010.5", "2011", "2012", "2013", "1", "215", "216", "2017", "-2018",
            "-2019", "-2020"},
    });
    std::vector<CreatureSpawnDataRow> rows = SpawnRepository::QueryCreatureSpawns();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].id, 1001u);
    CHECK_EQ(rows[0].map, 1002u);
    CHECK_EQ(rows[0].modelid, 1003u);
    CHECK_EQ(rows[0].equipmentId, 1004u);
    CHECK_EQ(rows[0].positionX, 1005.5f);
    CHECK_EQ(rows[0].positionY, 1006.5f);
    CHECK_EQ(rows[0].positionZ, 1007.5f);
    CHECK_EQ(rows[0].orientation, 1008.5f);
    CHECK_EQ(rows[0].spawntimesecs, 1009u);
    CHECK_EQ(rows[0].spawndist, 1010.5f);
    CHECK_EQ(rows[0].currentwaypoint, 1011u);
    CHECK_EQ(rows[0].curhealth, 1012u);
    CHECK_EQ(rows[0].curmana, 1013u);
    CHECK_EQ(rows[0].deathState, true);
    CHECK_EQ(rows[0].movementType, uint8(115));
    CHECK_EQ(rows[0].spawnMask, uint8(116));
    CHECK_EQ(rows[0].phaseMask, 1017u);
    CHECK_EQ(rows[0].gameEvent, int16(-1018));
    CHECK_EQ(rows[0].guidPoolId, int16(-1019));
    CHECK_EQ(rows[0].entryPoolId, int16(-1020));
    CHECK_EQ(rows[1].guid, 2000u);
}

// Each column of a game object spawn as the startup load reads it lands in its member, and the rows
// come back in order.
TEST(SpawnRepository_GameObjectSpawns)
{
    Db db(WorldDatabase);
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003.5", "1004.5", "1005.5", "1006.5", "1007.5", "1008.5",
            "1009.5", "1010.5", "-1011", "1012", "1013", "114", "1015", "-1016", "-1017",
            "-1018"},
        FakeRow{"2000", "2001", "2002", "2003.5", "2004.5", "2005.5", "2006.5", "2007.5", "2008.5",
            "2009.5", "2010.5", "-2011", "2012", "2013", "214", "2015", "-2016", "-2017",
            "-2018"},
    });
    std::vector<GameObjectSpawnDataRow> rows = SpawnRepository::QueryGameObjectSpawns();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].guid, 1000u);
    CHECK_EQ(rows[0].id, 1001u);
    CHECK_EQ(rows[0].map, 1002u);
    CHECK_EQ(rows[0].positionX, 1003.5f);
    CHECK_EQ(rows[0].positionY, 1004.5f);
    CHECK_EQ(rows[0].positionZ, 1005.5f);
    CHECK_EQ(rows[0].orientation, 1006.5f);
    CHECK_EQ(rows[0].rotation0, 1007.5f);
    CHECK_EQ(rows[0].rotation1, 1008.5f);
    CHECK_EQ(rows[0].rotation2, 1009.5f);
    CHECK_EQ(rows[0].rotation3, 1010.5f);
    CHECK_EQ(rows[0].spawntimesecs, -1011);
    CHECK_EQ(rows[0].animprogress, 1012u);
    CHECK_EQ(rows[0].state, 1013u);
    CHECK_EQ(rows[0].spawnMask, uint8(114));
    CHECK_EQ(rows[0].phaseMask, 1015u);
    CHECK_EQ(rows[0].gameEvent, int16(-1016));
    CHECK_EQ(rows[0].guidPoolId, int16(-1017));
    CHECK_EQ(rows[0].entryPoolId, int16(-1018));
    CHECK_EQ(rows[1].guid, 2000u);
}
