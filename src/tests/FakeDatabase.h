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

#ifndef MANGOS_TESTS_FAKEDATABASE_H
#define MANGOS_TESTS_FAKEDATABASE_H

/**
 * A database with no MySQL behind it, for the unit tests.
 *
 * These lived inside DatabaseConcurrencyTest.cpp until decoupling D7a needed them in two
 * more places. FakeConnection still does what that test needs -- it reports any overlap
 * between two threads inside the same connection, which is the thing that test asserts is
 * impossible -- and now also records every statement it is handed and can answer canned
 * rows, so a test can drive the real Query/AsyncPQuery/callback paths end to end.
 *
 * The fakes are handed to a real Database through AttachTestConnections(); the database
 * owns none of them, and every holder here detaches before it lets them go.
 */

// QueryResult.h first, and it has to be: Database.h names QueryResult and QueryNamedResult
// without declaring either, so it only compiles behind them (DatabaseEnv.h is what orders
// them for the server).
#include "Database/QueryResult.h"
#include "Database/Database.h"
#include "Database/SqlOperations.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/// One canned row: its fields, as the strings a MySQL result would hand back.
typedef std::vector<std::string> FakeRow;
typedef std::vector<FakeRow> FakeRows;

/**
 * @brief A QueryResult over rows held in memory.
 *
 * The base is constructible with (rowCount, fieldCount) and leaves NextRow() as its only
 * pure virtual, so this is all a result needs to be: a cursor over the rows, writing the
 * current one into a Field array the caller reads through Fetch(). The strings live in this
 * object, so the `const char*` a Field hands out stays valid until the result is deleted --
 * which is exactly the contract QueryResultMysql has with the MySQL result it owns.
 */
class FakeQueryResult : public QueryResult
{
    public:

        explicit FakeQueryResult(FakeRows rows)
            : QueryResult(uint64(rows.size()), rows.empty() ? 0u : uint32(rows[0].size())),
              m_rows(std::move(rows)), m_next(0)
        {
            m_fields.reset(new Field[mFieldCount ? mFieldCount : 1]);
            for (uint32 i = 0; i < mFieldCount; ++i)
            {
                m_fields[i].SetType(MYSQL_TYPE_STRING);
            }
        }

        bool NextRow() override
        {
            if (m_next >= m_rows.size())
            {
                mCurrentRow = NULL;
                return false;
            }

            FakeRow const& row = m_rows[m_next++];
            for (uint32 i = 0; i < mFieldCount; ++i)
            {
                m_fields[i].SetValue(i < row.size() ? row[i].c_str() : NULL);
            }
            mCurrentRow = m_fields.get();
            return true;
        }

    private:

        FakeRows m_rows;
        std::unique_ptr<Field[]> m_fields;
        size_t m_next;
};

/**
 * @brief A connection that answers from a table of canned results.
 *
 * Query() records the statement, then returns the first registered answer whose prefix the
 * statement starts with, or NULL when none matches -- the same "no rows, no result" answer
 * MySQLConnection::Query() gives, and it calls NextRow() once before returning for the same
 * reason the real one does: callers Fetch() the first row without advancing.
 */
class FakeConnection final : public SqlConnection
{
    public:

        explicit FakeConnection(Database& database)
            : SqlConnection(database)
        {
        }

        bool Initialize(const char*) override { return true; }

        /// Answer `rows` to any statement starting with `sqlPrefix`. Registration order is
        /// the match order; an empty `rows` means "this query finds nothing" (NULL).
        void Answer(std::string sqlPrefix, FakeRows rows)
        {
            m_answers.push_back(std::make_pair(std::move(sqlPrefix), std::move(rows)));
        }

        QueryResult* Query(const char* sql) override
        {
            Enter();
            queryEntered.store(true);
            Record(sql);
            if (coordinateEscape)
            {
                while (!escapeAttempting.load())
                    std::this_thread::yield();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(20));
            }
            else
            {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(1));
            }
            QueryResult* result = Canned(sql);
            Leave();
            return result;
        }

        QueryNamedResult* QueryNamed(const char* sql) override
        {
            Enter();
            Record(sql);
            Leave();
            return nullptr;
        }

        bool Execute(const char* sql) override
        {
            Enter();
            Record(sql);
            Leave();
            return true;
        }

        unsigned long escape_string(
            char* to, const char* from,
            unsigned long length) override
        {
            Enter();
            for (unsigned long i = 0; i < length; ++i)
                to[i] = from[i];
            to[length] = '\0';
            Leave();
            return length;
        }

        void Enter()
        {
            if (active.fetch_add(1) != 0)
                overlap.store(true);
        }

        void Leave()
        {
            active.fetch_sub(1);
        }

        /// Every statement handed to this connection, in order. Appended under the
        /// connection's own lock (Database takes it before calling in), so the
        /// concurrency tests may hammer it from eight threads without racing here.
        std::vector<std::string> executed;

        std::atomic<int> active{0};
        std::atomic<bool> overlap{false};
        std::atomic<bool> queryEntered{false};
        std::atomic<bool> escapeAttempting{false};
        bool coordinateEscape = false;

    private:

        void Record(const char* sql)
        {
            executed.push_back(sql ? sql : "");
        }

        QueryResult* Canned(const char* sql)
        {
            const std::string statement(sql ? sql : "");
            for (size_t i = 0; i < m_answers.size(); ++i)
            {
                std::string const& prefix = m_answers[i].first;
                if (statement.compare(0, prefix.size(), prefix) != 0)
                {
                    continue;
                }
                if (m_answers[i].second.empty())
                {
                    return nullptr;             // a query that finds nothing answers NULL
                }
                FakeQueryResult* result = new FakeQueryResult(m_answers[i].second);
                result->NextRow();              // as MySQLConnection::Query() does
                return result;
            }
            return nullptr;
        }

        std::vector<std::pair<std::string, FakeRows> > m_answers;
};

/**
 * @brief Attaches fakes to a database for a scope, and detaches however the scope is left.
 *
 * A REQUIRE that fires returns from the test case, so an explicit DetachTestConnections()
 * at the end of the body is not reached -- and a GLOBAL database would be left pointing at
 * stack objects that die at the closing brace, which every later test in the binary would
 * then use. The guard makes the failure mode of a failing assertion "one red test" rather
 * than "one red test and a corrupt process".
 *
 * Lived in TestSeamTest.cpp until decoupling D7b needed it for the handler tests as well.
 */
struct AttachedFakes
{
    AttachedFakes(Database& database, SqlConnection* query, SqlConnection* async,
                  SqlResultQueue* results)
        : m_database(database)
    {
        m_database.AttachTestConnections(query, async, results);
    }

    ~AttachedFakes() { m_database.DetachTestConnections(); }

    AttachedFakes(AttachedFakes const&) = delete;
    AttachedFakes& operator=(AttachedFakes const&) = delete;

    Database& m_database;
};

/**
 * @brief A Database wired to two FakeConnections and a real SqlResultQueue.
 *
 * Built through the production seam (AttachTestConnections), so what the tests exercise is
 * the real Query/escape_string/AsyncQuery/ProcessResultQueue code, not a re-implementation
 * of it. It detaches in its destructor: the database must not outlive the objects it was
 * handed, and must not try to delete them.
 */
class FakeDatabase final : public Database
{
    public:

        FakeDatabase()
            : m_query(new FakeConnection(*this)),
              m_async(new FakeConnection(*this)),
              m_results(new SqlResultQueue())
        {
            AttachTestConnections(m_query, m_async, m_results);
        }

        ~FakeDatabase()
        {
            DetachTestConnections();
            delete m_results;
            delete m_async;
            delete m_query;
        }

        /// The pooled query connection -- what Query/PQuery/escape_string go through.
        FakeConnection& Connection() { return *m_query; }

        /// The async connection -- what the delay thread's operations run against.
        FakeConnection& AsyncConnection() { return *m_async; }

    protected:

        SqlConnection* CreateConnection() override
        {
            return new FakeConnection(*this);
        }

    private:

        FakeConnection* m_query;
        FakeConnection* m_async;
        SqlResultQueue* m_results;
};

#endif
