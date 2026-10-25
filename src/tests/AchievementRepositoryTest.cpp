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

/// The achievements' repositories on the global character and world databases backed by fakes: the
/// startup reads run on the calling thread's query connection; the writes are queued in order with
/// their values substituted as the server would send them to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/achievements/AchievementRepository.h"
#include "persistence/world/achievements/AchievementDataRepository.h"

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
        size_t seen = 0;

        explicit Db(Database& db) : database(db), query(db), async(db),
            attached(db, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            database.ExecuteQueuedForTest();
            std::vector<std::string> out(async.executed.begin() + seen, async.executed.end());
            seen = async.executed.size();
            return out;
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

// The realm-wide reads of both databases run on the calling thread.
TEST(AchievementRepository_StartupReads)
{
    Db chars(CharacterDatabase);
    Db world(WorldDatabase);
    delete AchievementRepository::QueryCompletedAchievements();
    delete AchievementRepository::QueryProgressCriteria();
    delete AchievementDataRepository::QueryCriteriaRequirements();
    delete AchievementDataRepository::QueryRewards();
    delete AchievementDataRepository::QueryRewardLocales();
    CheckLines(chars.query.executed, {
        "SELECT `achievement` FROM `character_achievement` GROUP BY `achievement`",
        "SELECT DISTINCT `criteria` FROM `character_achievement_progress`",
    }, __LINE__);
    REQUIRE(world.query.executed.size() == size_t(3));
    CHECK_STR(world.query.executed[0], "SELECT `criteria_id`, `type`, `value1`, `value2` FROM `achievement_criteria_requirement`");
    CHECK_STR(world.query.executed[1], "SELECT `entry`, `gender`, `title_A`, `title_H`, `item`, `sender`, `subject`, `text` FROM `achievement_reward`");
    CHECK_STR(world.query.executed[2], "SELECT `entry`,`gender`,"
                                       "`subject_loc1`,`text_loc1`,`subject_loc2`,`text_loc2`,"
                                       "`subject_loc3`,`text_loc3`,`subject_loc4`,`text_loc4`,"
                                       "`subject_loc5`,`text_loc5`,`subject_loc6`,`text_loc6`,"
                                       "`subject_loc7`,`text_loc7`,`subject_loc8`,`text_loc8`,"
                                       "`subject_loc9`,`text_loc9`,`subject_loc10`,`text_loc10`,"
                                       "`subject_loc11`,`text_loc11`"
                                       " FROM `locales_achievement_reward`");
}

// The writes, the delete of a character's rows as one transaction.
TEST(AchievementRepository_Writes)
{
    Db db(CharacterDatabase);
    {
        TickGuard::Scope scope;
        AchievementRepository::DeleteAllOf(42);
        AchievementRepository::ReplaceCompleted(42, 6, 1700000000);
        AchievementRepository::DeleteCompleted(42, 7);
        AchievementRepository::DeleteProgress(42, 100);
        AchievementRepository::InsertProgress(42, 100, 3, 1700000001);
        AchievementRepository::DeleteAchievementOfEveryCharacter(8);
        AchievementRepository::DeleteCriteriaOfEveryCharacter(101);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(8));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `character_achievement` WHERE `guid` = 42",
        "DELETE FROM `character_achievement_progress` WHERE `guid` = 42",
        "DELETE FROM `character_achievement` WHERE `guid` = '42' AND `achievement` = '6'",
        "INSERT INTO `character_achievement` (`guid`, `achievement`, `date`) VALUES ('42', '6', '1700000000')",
        "DELETE FROM `character_achievement` WHERE `guid` = 42 AND `achievement` = 7",
        "DELETE FROM `character_achievement_progress` WHERE `guid` = '42' AND `criteria` = '100'",
        "INSERT INTO `character_achievement_progress` (`guid`, `criteria`, `counter`, `date`) VALUES ('42', '100', '3', '1700000001')",
        "DELETE FROM `character_achievement` WHERE `achievement` = 8",
        "DELETE FROM `character_achievement_progress` WHERE `criteria` = 101",
    }, __LINE__);
}
