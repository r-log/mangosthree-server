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

/// Decoupling D1: a Database that was never Initialize()d refuses instead of crashing.
/// Before this test, Query() on such a database indexed an empty connection vector
/// (Database.cpp:314) and locked the garbage it read (Database.h:185).

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"

#include <string>

TEST(NullDatabase_IsFalseBeforeInitialize)
{
    DatabaseMysql db;
    CHECK(!db);
}

TEST(NullDatabase_QueryAnswersNull)
{
    DatabaseMysql db;
    CHECK(db.Query("SELECT 1") == NULL);
    CHECK(db.QueryNamed("SELECT 1") == NULL);
    CHECK(db.PQuery("SELECT %u", 1u) == NULL);
    CHECK(db.PQueryNamed("SELECT %u", 1u) == NULL);
}

TEST(NullDatabase_ExecuteAndAsyncAnswerFalse)
{
    DatabaseMysql db;
    CHECK(!db.Execute("SELECT 1"));
    CHECK(!db.PExecute("SELECT %u", 1u));
    CHECK(!db.AsyncQuery([](QueryResult*) {}, "SELECT 1"));
    CHECK(!db.AsyncPQuery([](QueryResult*) {}, "SELECT %u", 1u));
}

// Debate F6: two more public paths index the empty pool. escape_string locks
// m_pQueryConnections[0] for any non-empty string (Database.cpp:296); Ping locks m_pAsyncConn
// and then the pool by its default size of one (Database.cpp:322-328).
TEST(NullDatabase_EscapeLeavesTheStringUntouched)
{
    DatabaseMysql db;
    std::string value("it's");
    db.escape_string(value);
    CHECK_STR(value, "it's");
}

TEST(NullDatabase_PingReturns)
{
    DatabaseMysql db;
    db.Ping();
    CHECK(!db);
}

// Decoupling D7a, fix round 1. D1 guarded "never Initialize()d", where everything is NULL
// together. HaltDelayThread() leaves a second, asymmetric state: the async connection is still
// alive while the per-thread transaction slot and the worker are gone. Nothing used to run in
// that window, because the callbacks queued during the halt were discarded -- D7a's shutdown
// drain now runs them, and a callback that writes (World::_UpdateRealmCharCount reaching
// LoginDatabase.PExecute) dereferenced the NULL slot on its way to Execute(). Every write path
// has to refuse there, which is what the old discard amounted to anyway.
TEST(HaltedDatabase_WritesAreRefusedInsteadOfCrashing)
{
    FakeDatabase db;

    // Attached, the write paths work.
    CHECK(db.Execute("UPDATE `characters` SET `online` = 0"));
    CHECK(db.PExecute("UPDATE `characters` SET `online` = %u", 0u));
    CHECK(db.BeginTransaction());
    CHECK(db.CommitTransactionDirect());        // leaves the transaction slot clean again

    db.HaltDelayThreadForTest();                // exactly what StopDatabases() leaves behind

    // The connection is still there; the slot and the worker are not.
    CHECK(bool(db));
    CHECK(!db.Execute("UPDATE `characters` SET `online` = 0"));
    CHECK(!db.PExecute("UPDATE `characters` SET `online` = %u", 0u));
    CHECK(!db.PExecuteLog("UPDATE `characters` SET `online` = %u", 0u));
    CHECK(!db.BeginTransaction());
    CHECK(!db.CommitTransaction());
    CHECK(!db.CommitTransactionDirect());
    CHECK(!db.CommitTransactionChecked());
    CHECK(!db.RollbackTransaction());

    // The prepared-statement path too. SqlStatement::Execute() detaches its parameters and
    // hands ownership over, so the refusal frees them rather than leaking them.
    SqlStatementID id;
    SqlStatement queued = db.CreateStatement(id, "UPDATE `characters` SET `online` = ?");
    queued.addUInt32(0);
    CHECK(!queued.Execute());
}

// The same D1 treatment for the one point that had no null guard at all: DirectExecuteStmt
// locked getAsyncConnection() without asking whether it was NULL.
TEST(NullDatabase_DirectStatementAnswersFalse)
{
    DatabaseMysql db;

    SqlStatementID id;
    SqlStatement direct = db.CreateStatement(id, "UPDATE `characters` SET `online` = ?");
    direct.addUInt32(0);
    CHECK(!direct.DirectExecute());

    SqlStatement queued = db.CreateStatement(id, "UPDATE `characters` SET `online` = ?");
    queued.addUInt32(0);
    CHECK(!queued.Execute());

    CHECK(!db);
}
