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

/// The guild charters' repository on the global CharacterDatabase backed by fakes: the reads and
/// holders are queued, not run on the calling thread, and answered on the world thread; the writes
/// reach the database in order, the new charter and the delete as one queued transaction each.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "Database/TickGuard.h"
#include "persistence/characters/social/PetitionRepository.h"

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
        size_t seen = 0;

        Db() : query(CharacterDatabase), async(CharacterDatabase),
            attached(CharacterDatabase, &query, &async, &results, /*asyncWrites*/ true)
        {
            TickGuard::ResetViolations();
        }

        std::vector<std::string> Take()
        {
            CharacterDatabase.ExecuteQueuedForTest();
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

// The four single reads and the three holders are queued, run in order, and each callback runs once
// when the world thread drains the result queue.
TEST(PetitionRepository_ReadsAreAsynchronous)
{
    Db db;
    int answered = 0;
    PetitionReadCallback read = [&answered](QueryResult* result) { delete result; ++answered; };
    PetitionHolderCallback held = [&answered](QueryResult* /*result*/, SqlQueryHolder* holder) { delete holder; ++answered; };
    {
        TickGuard::Scope scope;
        PetitionRepository::QueryOwnedPetitions(42, read);
        PetitionRepository::QueryExists(7, read);
        PetitionRepository::QueryOwner(7, read);
        PetitionRepository::QueryDetails(7, read);
        PetitionRepository::QueueShowSignatures(7, held);
        PetitionRepository::QueueSignRead(7, 3, held);
        PetitionRepository::QueueTurnInRead(7, held);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(db.query.executed.size(), size_t(0));
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(7));
    }
    CheckLines(db.Take(), {
        "SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '42'",
        "SELECT 1 FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid`, `name`,   (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '7') AS `signs` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT 1 FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '7'",
        "SELECT `ownerguid`,   (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '7') AS `signs` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '3'",
        "SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '7'",
        "SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '7'",
    }, __LINE__);
    CHECK_EQ(answered, 0);
    CharacterDatabase.ProcessResultQueue();
    CHECK_EQ(answered, 7);
}

// The new charter's delete list, then its transaction; the rename; the delete's transaction.
TEST(PetitionRepository_Writes)
{
    Db db;
    CHECK_STR(PetitionRepository::GuidList({ 4, 5 }, 9), "'4' , '5' , '9'");
    CHECK_STR(PetitionRepository::GuidList({}, 9), "'9'");
    {
        TickGuard::Scope scope;
        PetitionRepository::Create(42, 9, "Wardens", PetitionRepository::GuidList({ 4 }, 9));
        PetitionRepository::Rename(9, "Watchers");
        PetitionRepository::Delete(9);
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(CharacterDatabase.GetDelayQueueDepth(), size_t(3));
    }
    CheckLines(db.Take(), {
        "DELETE FROM `petition` WHERE `petitionguid` IN ( '4' , '9' )",
        "DELETE FROM `petition_sign` WHERE `petitionguid` IN ( '4' , '9' )",
        "INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES ('42', '9', 'Wardens')",
        "UPDATE `petition` SET `name` = 'Watchers' WHERE `petitionguid` = '9'",
        "DELETE FROM `petition` WHERE `petitionguid` = '9'",
        "DELETE FROM `petition_sign` WHERE `petitionguid` = '9'",
    }, __LINE__);
}
