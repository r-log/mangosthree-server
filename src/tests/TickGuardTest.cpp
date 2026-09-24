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

/// Decoupling D7a: the tick guard counts what the tick waits for. Each case here runs the
/// real Database entry points against fake connections, so what is asserted is the
/// production counting path, not a model of it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"

#include <string>
#include <thread>

TEST(TickGuard_QueryOutsideAScopeIsNotCounted)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    CHECK(!TickGuard::Active());
    database.PQuery("SELECT %u", 1u);

    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(TickGuard_QueryInsideAScopeIsCountedOnce)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    {
        TickGuard::Scope scope;
        CHECK(TickGuard::Active());
        // A PQuery formats and calls Query, which is one acquisition, not two.
        database.PQuery("SELECT %u FROM `characters`", 1u);
    }

    CHECK(!TickGuard::Active());
    CHECK_EQ(TickGuard::Violations(), 1u);
}

TEST(TickGuard_EscapeStringInsideAScopeIsCounted)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    {
        TickGuard::Scope scope;
        std::string value("account'name");
        database.escape_string(value);
    }

    CHECK_EQ(TickGuard::Violations(), 1u);
}

TEST(TickGuard_AsyncQueryInsideAScopeIsNotCountedAndRunsLater)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    bool callbackRan = false;
    {
        TickGuard::Scope scope;
        CHECK(database.AsyncPQuery([&callbackRan](QueryResult* /*result*/) { callbackRan = true; },
                                   "SELECT `guid` FROM `characters` WHERE `account` = %u", 7u));

        // Queued, not run: nothing has touched a connection yet, so nothing is counted.
        CHECK_EQ(TickGuard::Violations(), 0u);
        CHECK_EQ(database.AsyncConnection().executed.size(), size_t(0));
        CHECK(!callbackRan);
        CHECK_EQ(database.GetDelayQueueDepth(), size_t(1));
    }

    // What the delay thread would have done, then what the world thread does with the
    // callback it left behind.
    database.ExecuteQueuedForTest();
    CHECK_EQ(database.GetDelayQueueDepth(), size_t(0));
    CHECK_EQ(database.AsyncConnection().executed.size(), size_t(1));
    CHECK_STR(database.AsyncConnection().executed[0],
              "SELECT `guid` FROM `characters` WHERE `account` = 7");
    CHECK(!callbackRan);

    database.ProcessResultQueue();
    CHECK(callbackRan);

    // The worker's own acquisition is not the tick's: it ran outside any scope.
    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(TickGuard_IsPerThreadNotPerProcess)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    TickGuard::Scope scope;                     // held by THIS thread for the whole case
    std::thread other([&database]()
    {
        // A network thread or the delay thread: it holds no scope of its own, and the
        // scope the main thread holds is none of its business.
        CHECK(!TickGuard::Active());
        database.PQuery("SELECT %u", 2u);
    });
    other.join();

    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(TickGuard_NullDatabaseInsideAScopeCountsNothing)
{
    TickGuard::ResetViolations();
    DatabaseMysql database;                     // never Initialize()d

    {
        TickGuard::Scope scope;
        // The D1 null guard returns before the acquisition, and the count sits after it:
        // a query that never reaches a connection never waited on one.
        CHECK(database.Query("SELECT 1") == NULL);
        CHECK(database.PQuery("SELECT %u", 1u) == NULL);
        database.Ping();

        // The same for the prepared-statement direct path, whose null guard D7a fix 1 added:
        // it refuses before the count, so an un-initialised database still counts nothing.
        SqlStatementID id;
        SqlStatement direct = database.CreateStatement(id, "UPDATE `characters` SET `online` = ?");
        direct.addUInt32(0);
        CHECK(!direct.DirectExecute());
    }

    CHECK_EQ(TickGuard::Violations(), 0u);
}

TEST(TickGuard_ResetViolationsClearsTheCount)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    {
        TickGuard::Scope scope;
        database.PQuery("SELECT %u", 3u);
        database.PQuery("SELECT %u", 4u);
    }
    CHECK_EQ(TickGuard::Violations(), 2u);

    TickGuard::ResetViolations();
    CHECK_EQ(TickGuard::Violations(), 0u);
}
