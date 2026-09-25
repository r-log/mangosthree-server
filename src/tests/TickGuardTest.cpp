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
///
/// Decoupling D7h: four of these cases exist to provoke a violation, and under
/// MANGOS_STRICT_TICK a violation aborts the process -- the binary would die on the first
/// of them instead of reporting. Each stands down through SkippedUnderStrict(), which is
/// the reason TickGuard::Strict() is in the header at all, and says so on stdout so a
/// strict run's output is not mistaken for a full one. What the strict build proves
/// instead is the abort itself, and that is proved by inducing one in a server (the PR's
/// induced-failure run), not by a unit test that would have to survive its own abort.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Database/TickGuard.h"

#include <cstdio>
#include <string>
#include <thread>

namespace
{
    /// Whether this case must stand down because the guard is armed, and a line saying so
    /// when it does. A silent `return` would make a strict run's `ok` indistinguishable
    /// from a real pass, which is exactly the reassurance a strict CI leg must not give.
    /// `name` is the case's own name: the TEST macro defines the body as a function called
    /// after the case, so __func__ cannot drift from it.
    bool SkippedUnderStrict(const char* name)
    {
        if (!TickGuard::Strict())
        {
            return false;
        }
        std::printf("  skip %s (strict build: this case provokes a violation, which aborts)\n", name);
        return true;
    }
}

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
    if (SkippedUnderStrict(__func__))
    {
        return;
    }

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
    if (SkippedUnderStrict(__func__))
    {
        return;
    }

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
    if (SkippedUnderStrict(__func__))
    {
        return;
    }

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

/// Decoupling D7h. An administrative reload (`.reload <table>`) runs a start-up loader
/// synchronously inside World::Update, deliberately. Under an AdminScope those
/// acquisitions go to their own counter, leave the tick's count alone, and -- the reason
/// the scope exists at all -- do not assert, so this case runs in a strict build too.
TEST(TickGuard_AdminScopeCountsApartAndDoesNotAssert)
{
    TickGuard::ResetViolations();
    FakeDatabase database;

    CHECK(!TickGuard::AdminActive());
    {
        TickGuard::Scope scope;                 // the world tick, as always
        TickGuard::AdminScope admin(true);      // ...inside which a reload handler runs
        CHECK(TickGuard::Active());
        CHECK(TickGuard::AdminActive());

        // `.reload all` calls one loader after another, and a nested scope must not end
        // the outer one: the depth is what makes that true.
        {
            TickGuard::AdminScope nested(true);
            CHECK(TickGuard::AdminActive());
            database.PQuery("SELECT %u FROM `spell_chain`", 1u);
        }
        CHECK(TickGuard::AdminActive());
        std::string value("name'");
        database.escape_string(value);
    }
    CHECK(!TickGuard::AdminActive());

    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(TickGuard::AdminViolations(), 2u);

    // And both counters reset together.
    TickGuard::ResetViolations();
    CHECK_EQ(TickGuard::Violations(), 0u);
    CHECK_EQ(TickGuard::AdminViolations(), 0u);
}

/// A scope constructed with `false` is the dispatcher's non-reload case: it must not
/// suppress anything, or every chat command would stop being counted.
TEST(TickGuard_AdminScopeNotEnteredSuppressesNothing)
{
    if (SkippedUnderStrict(__func__))
    {
        return;
    }

    TickGuard::ResetViolations();
    FakeDatabase database;

    {
        TickGuard::Scope scope;
        TickGuard::AdminScope admin(false);
        CHECK(!TickGuard::AdminActive());
        database.PQuery("SELECT %u FROM `characters`", 5u);
    }

    CHECK_EQ(TickGuard::Violations(), 1u);
    CHECK_EQ(TickGuard::AdminViolations(), 0u);
}

/// Decoupling D7h. TickGuard::Strict() must answer what this build was configured with.
/// It reaches the guard through shared_db's PUBLIC compile definition; the expectation it
/// is compared against is put on the test target directly by src/tests/CMakeLists.txt from
/// the same CMake option. Two independent paths from one switch: if the definition stops
/// propagating through the library -- a PRIVATE where a PUBLIC was, a target that stops
/// linking shared_db -- the two disagree and this fails, instead of the guard silently
/// going quiet in a build that was asked to arm it.
TEST(TickGuard_StrictMatchesTheBuildConfiguration)
{
#ifdef MANGOS_TESTS_STRICT_TICK_CONFIGURED
    CHECK(TickGuard::Strict());
#else
    CHECK(!TickGuard::Strict());
#endif
}
