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

/// The quest tracker's repository on the global CharacterDatabase backed by fakes: every write in
/// order with its values substituted as the server would send them to MySQL.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"
#include "persistence/characters/quests/QuestTrackerRepository.h"

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
    };
}

// The acceptance, then the three stamps on the latest acceptance, each one queued statement.
TEST(QuestTrackerRepository_Writes)
{
    Db db;
    {
        TickGuard::Scope scope;
        QuestTrackerRepository::InsertAccept(26, 42, "abc123", "2026-10-25");
        QuestTrackerRepository::SetCompleteTime(26, 42);
        QuestTrackerRepository::SetAbandonTime(27, 42);
        QuestTrackerRepository::SetCompletedByGm(28, 42);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(4));
    }
    CharacterDatabase.ExecuteQueuedForTest();
    std::vector<std::string> const want = {
        "INSERT INTO `quest_tracker` (`id`, `character_guid`, `quest_accept_time`, `core_hash`, `core_revision`) VALUES ('26', '42', NOW(), 'abc123', '2026-10-25')",
        "UPDATE `quest_tracker` SET `quest_complete_time` = NOW() WHERE `id` = '26' AND `character_guid` = '42' ORDER BY `quest_accept_time` DESC LIMIT 1",
        "UPDATE `quest_tracker` SET `quest_abandon_time` = NOW() WHERE `id` = '27' AND `character_guid` = '42' ORDER BY `quest_accept_time` DESC LIMIT 1",
        "UPDATE `quest_tracker` SET `completed_by_gm` = 1 WHERE `id` = '28' AND `character_guid` = '42' ORDER BY `quest_accept_time` DESC LIMIT 1",
    };
    REQUIRE(db.async.executed.size() == want.size());
    for (size_t i = 0; i < want.size(); ++i)
    {
        CHECK_STR(db.async.executed[i], want[i]);
    }
    CHECK_EQ(db.query.executed.size(), size_t(0));
}
