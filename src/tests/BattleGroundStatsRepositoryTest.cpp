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

/// The battleground statistics', saved variables' and instance script data's repositories on the
/// global CharacterDatabase backed by fakes: the queued writes in order with their values
/// substituted, and the startup reads on the calling thread's connection.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/SavedVariablesRepository.h"
#include "persistence/characters/maps/InstanceDataRepository.h"
#include "persistence/characters/pvp/BattleGroundStatsRepository.h"

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

// A finished battleground's row and its players' scores are queued, then the next arena point
// distribution time.
TEST(BattleGroundStatsRepository_Writes)
{
    Db db;
    {
        TickGuard::Scope scope;
        BattleGroundStatsRepository::InsertBattleGround(5, 1, 8, 3);

        BattleGroundPlayerStatsRow row;
        row.battleGroundId = 5;
        row.guid = 42;
        row.killingBlows = 1;
        row.deaths = 2;
        row.honorableKills = 3;
        row.bonusHonor = 4;
        row.damageDone = 5000;
        row.healingDone = 6000;
        row.attr1 = 7;
        row.attr2 = 8;
        row.attr3 = 9;
        row.attr4 = 10;
        row.attr5 = 11;
        BattleGroundStatsRepository::InsertPlayer(row);

        SavedVariablesRepository::InsertNextArenaPointDistribution(1700000000);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "INSERT INTO pvpstats_battlegrounds (id, winner_team, bracket_id, type, date) VALUES ('5', '1', '8', '3', NOW())",
        "INSERT INTO `pvpstats_players` (`battleground_id`, `character_guid`, `score_killing_blows`, `score_deaths`, `score_honorable_kills`, `score_bonus_honor`, `score_damage_done`, `score_healing_done`, `attr_1`, `attr_2`, `attr_3`, `attr_4`, `attr_5`) VALUES ('5', '42', '1', '2', '3', '4', '5000', '6000', '7', '8', '9', '10', '11')",
        "INSERT INTO `saved_variables` (`NextArenaPointDistributionTime`) VALUES ('1700000000')",
    }, __LINE__);
}

// The startup reads run on the calling thread, in call order.
TEST(BattleGroundStatsRepository_StartupReads)
{
    Db db;
    delete BattleGroundStatsRepository::QueryHighestId();
    delete SavedVariablesRepository::QueryNextArenaPointDistribution();
    delete InstanceDataRepository::QueryInstanceData();
    delete InstanceDataRepository::QueryWorldData();
    CheckLines(db.query.executed, {
        "SELECT MAX(`id`) FROM `pvpstats_battlegrounds`",
        "SELECT `NextArenaPointDistributionTime` FROM `saved_variables`",
        "SELECT `id`, `map`, `data` FROM `instance`",
        "SELECT `map`, `data` FROM `world`",
    }, __LINE__);
    CHECK_EQ(db.async.executed.size(), size_t(0));
}

// An instance's and a world map's script data saves are queued, the data bound, then a world
// map's empty row.
TEST(InstanceDataRepository_Writes)
{
    Db db;
    {
        TickGuard::Scope scope;
        InstanceDataRepository::SetInstanceData("1 0 2", 3);
        InstanceDataRepository::SetWorldData("4 5", 571);
        InstanceDataRepository::InsertEmptyWorldData(530);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "UPDATE `instance` SET `data` = '1 0 2' WHERE `id` = '3'",
        "UPDATE `world` SET `data` = '4 5' WHERE `map` = '571'",
        "INSERT INTO `world` VALUES ('530', '')",
    }, __LINE__);
}
