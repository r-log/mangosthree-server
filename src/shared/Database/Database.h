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

#ifndef DATABASE_H
#define DATABASE_H

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include "Threading/Threading.h"
#include "Database/SqlDelayThread.h"
#include "Database/TickGuard.h"
#include "Threading/ThreadLocalStore.h"

#include <atomic>
#include <mutex>
#include "SqlPreparedStatement.h"

class SqlTransaction;
class SqlResultQueue;
class SqlQueryHolder;
class SqlStmtParameters;
class SqlParamBinder;
class Database;

#define MAX_QUERY_LEN   (32*1024)

/**
 * @brief Did vsnprintf fail to lay the whole query into a MAX_QUERY_LEN buffer?
 *
 * Every P* entry point formats into a stack buffer of that size, and each of them
 * used to test `res == -1`. vsnprintf does not report truncation that way: it returns
 * the length the query WOULD have needed, and goes negative only on an encoding
 * error. That is C99, and MSVC has conformed since 2015 -- the -1 belongs to the old
 * _vsnprintf. So the guard never fired, and a query longer than 32 KB was cut at the
 * buffer and handed to the server anyway, under a log line promising it had not been.
 * Truncation lands mid-statement, so what MySQL reports is a syntax error on a save
 * that silently did not happen.
 *
 * Shared rather than repeated, so the seven callers cannot drift apart again. Logs the
 * reason itself; the caller only has to return its own kind of failure.
 */
bool QueryFormatFailed(int res, const char* format);

enum DatabaseTypes
{
    DATABASE_WORLD,
    DATABASE_REALMD,
    DATABASE_CHARACTER,
    COUNT_DATABASES,
};

/**
 * @brief Abstract base class for database connections
 *
 * SqlConnection provides the interface for all database operations in MaNGOS.
 * It handles connection management, query execution, and transaction support.
 * This is the base class that specific database implementations (MySQL, etc.)
 * must inherit from.
 *
 * Features:
 * - Database connection initialization and management
 * - SQL query execution with result handling
 * - Transaction support (begin/commit/rollback)
 * - Prepared statement support for performance
 * - Thread-safe operations with locking mechanism
 * - String escaping for SQL injection prevention
 *
 * @note This is an abstract class - use concrete implementations like DatabaseMysql
 * @note All database operations should use the Lock class for thread safety
 */
class SqlConnection
{
    public:
        /**
         * @brief Virtual destructor for proper cleanup of derived classes
         */
        virtual ~SqlConnection() {}

        /**
         * @brief Initialize database connection with connection string
         * @param infoString Database connection string (host:port,user,password,database)
         * @return true if connection successful, false otherwise
         */
        virtual bool Initialize(const char* infoString) = 0;

        /**
         * @brief Execute SQL query and return results
         *
         * This method executes a SELECT query and returns the result set.
         * Used for queries that return data (SELECT, SHOW, etc.).
         *
         * @param sql SQL query string to execute
         * @return QueryResult pointer containing result data, NULL if error
         */
        virtual QueryResult* Query(const char* sql) = 0;
        /**
         * @brief
         *
         * @param sql
         * @return QueryNamedResult
         */
        virtual QueryNamedResult* QueryNamed(const char* sql) = 0;

        /**
         * @brief public methods for making requests
         *
         * @param sql
         * @return bool
         */
        virtual bool Execute(const char* sql) = 0;

        /**
         * @brief escape string generation
         *
         * @param to
         * @param from
         * @param length
         * @return unsigned long
         */
        virtual unsigned long escape_string(char* to, const char* from, unsigned long length) { strncpy(to, from, length); return length; }

        /**
         * @brief nothing do if DB not support transactions
         *
         * @return bool
         */
        virtual bool BeginTransaction() { return true; }
        /**
         * @brief
         *
         * @return bool
         */
        virtual bool CommitTransaction() { return true; }
        /**
         * @brief can't rollback without transaction support
         *
         * @return bool
         */
        virtual bool RollbackTransaction() { return true; }

        /**
         * @brief methods to work with prepared statements
         *
         * @param nIndex
         * @param id
         * @return bool
         */
        bool ExecuteStmt(int nIndex, const SqlStmtParameters& id);

        /**
         * @brief SqlConnection object lock
         *
         */
        class Lock
        {
            public:
                /**
                 * @brief
                 *
                 * @param conn
                 */
                Lock(SqlConnection* conn) : m_pConn(conn) { m_pConn->m_mutex.lock(); }
                /**
                 * @brief
                 *
                 */
                ~Lock() { m_pConn->m_mutex.unlock(); }

                /**
                 * @brief
                 *
                 * @return SqlConnection *operator ->
                 */
                SqlConnection* operator->() const { return m_pConn; }

            private:
                SqlConnection* const m_pConn; /**< TODO */
        };

        /**
         * @brief get DB object
         *
         * @return Database
         */
        Database& DB() { return m_db; }

    protected:
        /**
         * @brief
         *
         * @param db
         */
        SqlConnection(Database& db) : m_db(db) {}

        /**
         * @brief
         *
         * @param fmt
         * @return SqlPreparedStatement
         */
        virtual SqlPreparedStatement* CreateStatement(const std::string& fmt);
        /**
         * @brief allocate prepared statement and return statement ID
         *
         * @param nIndex
         * @return SqlPreparedStatement
         */
        SqlPreparedStatement* GetStmt(uint32 nIndex);

        Database& m_db; /**< TODO */

        /**
         * @brief free prepared statements objects
         *
         */
        void FreePreparedStatements();

    private:
        // A plain mutex, not a recursive one. SqlTransaction takes this lock once for
        // the whole BEGIN..COMMIT and runs each queued statement through
        // ExecuteLocked(), so nothing re-enters it. It used to be recursive because
        // every statement locked the connection again on its way through Execute();
        // making it recursive once more would hide that design error rather than fix it.
        typedef std::mutex LOCK_TYPE;
        LOCK_TYPE m_mutex;

        /**
         * @brief
         *
         */
        typedef std::vector<SqlPreparedStatement* > StmtHolder;
        StmtHolder m_holder; /**< TODO */
};

/**
 * @brief
 *
 */
class Database
{
    public:
        /**
         * @brief
         *
         */
        virtual ~Database();

        /**
         * @brief
         *
         * @param infoString
         * @param nConns
         * @return bool
         */
        virtual bool Initialize(const char* infoString, int nConns = 1);
        /**
         * @brief start worker thread for async DB request execution
         *
         */
        virtual void InitDelayThread();
        /**
         * @brief stop worker thread
         *
         */
        virtual void HaltDelayThread();

        /**
         * @brief Runs a query on one of the pooled connections.
         *
         * A database that was never Initialize()d has no pool; asking it answers NULL, the
         * same as a failed query, instead of indexing the empty pool (decoupling D1).
         *
         * @param sql
         * @return QueryResult
         */
        inline QueryResult* Query(const char* sql)
        {
            if (m_pQueryConnections.empty())
            {
                return NULL;
            }
            // The SQL is handed to getQueryConnection() only so the tick guard can name
            // the statement it counted (decoupling D7a); the connection choice ignores it.
            SqlConnection::Lock guard(getQueryConnection(sql));
            return guard->Query(sql);
        }

        /**
         * @brief Same as Query, with named columns. NULL on a database with no pool.
         *
         * @param sql
         * @return QueryNamedResult
         */
        inline QueryNamedResult* QueryNamed(const char* sql)
        {
            if (m_pQueryConnections.empty())
            {
                return NULL;
            }
            SqlConnection::Lock guard(getQueryConnection(sql));
            return guard->QueryNamed(sql);
        }

        /**
         * @brief
         *
         * @param format...
         * @return QueryResult
         */
        QueryResult* PQuery(const char* format, ...) ATTR_PRINTF(2, 3);
        /**
         * @brief
         *
         * @param format...
         * @return QueryNamedResult
         */
        QueryNamedResult* PQueryNamed(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        inline bool DirectExecute(const char* sql)
        {
            if (!m_pAsyncConn)
            {
                return false;
            }

            // The whole direct-execute family is counted here, once per call: DirectPExecute
            // formats and comes straight through, and Execute() falls back to this when async
            // transactions are not allowed yet. Counting in DirectPExecute as well would count
            // one acquisition twice.
            if (TickGuard::Active())
            {
                TickGuard::Violation(sql);
            }

            SqlConnection::Lock guard(m_pAsyncConn);
            return guard->Execute(sql);
        }

        /**
         * @brief
         *
         * @param format...
         * @return bool
         */
        bool DirectPExecute(const char* format, ...) ATTR_PRINTF(2, 3);


        // Query / member
        /// Async queries and query holders (Database.cpp). The callback runs
        /// on whichever thread later calls ProcessResultQueue() (typically
        /// the thread that issued the query), not the worker thread that ran
        /// the SQL -- bind whatever object/state it needs into the lambda.

        /// Runs sql on a worker thread and invokes callback(result) once done.
        bool AsyncQuery(std::function<void(QueryResult*)> callback, const char* sql);

        /// printf-style AsyncQuery(): the query text is formatted immediately
        /// (on the calling thread), only execution is deferred.
        bool AsyncPQuery(std::function<void(QueryResult*)> callback, const char* format, ...) ATTR_PRINTF(3, 4);

        /// Runs every query already staged in holder on a worker thread, then
        /// invokes callback(nullptr, holder) once all of them complete --
        /// results are retrieved from the holder itself (SqlQueryHolder::GetResult()).
        bool DelayQueryHolder(std::function<void(QueryResult*, SqlQueryHolder*)> callback, SqlQueryHolder* holder);


        // Query / static
        // PQuery / member
        // PQuery / static

        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        bool Execute(const char* sql);
        /**
         * @brief
         *
         * @param format...
         * @return bool
         */
        bool PExecute(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief Writes SQL commands to a LOG file (see mangosd.conf "LogSQL")
         *
         * @param format...
         * @return bool
         */
        bool PExecuteLog(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief
         *
         * @return bool
         */
        bool BeginTransaction();
        /**
         * @brief
         *
         * @return bool
         */
        bool CommitTransaction();
        /**
         * @brief
         *
         * @return bool
         */
        bool RollbackTransaction();
        /**
         * @brief for sync transaction execution
         *
         * @return bool
         */
        bool CommitTransactionDirect();

        /**
         * @brief Commit through the delay thread and block for the REAL result.
         *
         * CommitTransaction() reports only that the transaction was queued;
         * CommitTransactionDirect() runs it but discards the result. Use this where the
         * answer matters -- anything moving items or money.
         *
         * @return bool whether the transaction actually committed
         */
        bool CommitTransactionChecked();

        // PREPARED STATEMENT API
        /**
         * @brief allocate index for prepared statement with SQL request 'fmt'
         *
         * @param index
         * @param fmt
         * @return SqlStatement
         */
        SqlStatement CreateStatement(SqlStatementID& index, const char* fmt);
        /**
         * @brief get prepared statement format string
         *
         * @param stmtId
         * @return std::string
         */
        std::string GetStmtString(const int stmtId) const;

        /**
         * @brief
         *
         * @return operator
         */
        operator bool () const { return m_pQueryConnections.size() && m_pAsyncConn != 0; }

        /**
         * @brief escape string generation
         *
         * @param str
         */
        void escape_string(std::string& str);

        /**
         * @brief must be called before first query in thread (one time for thread using one from existing Database objects)
         *
         */
        virtual void ThreadStart();
        /**
         * @brief must be called before finish thread run (one time for thread using one from existing Database objects)
         *
         */
        virtual void ThreadEnd();

        /**
         * @brief set database-wide result queue. also we should use object-bases and not thread-based result queues
         *
         */
        void ProcessResultQueue();

        /**
        * @brief Function to check that the database version matches expected core version
        *
        * @param DatabaseTypes
        * @return bool
        */
        bool CheckDatabaseVersion(DatabaseTypes database);
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetPingIntervall() { return m_pingIntervallms; }

        /**
         * @brief function to ping database connections
         *
         */
        void Ping();

        /**
         * @brief set this to allow async transactions
         *
         * you should call it explicitly after your server successfully started
         * up.
         * NO ASYNC TRANSACTIONS DURING SERVER STARTUP - ONLY DURING RUNTIME!!!
         *
         */
        void AllowAsyncTransactions() { m_bAllowAsyncTransactions = true; }

        /**
         * @brief Refuse further async work, so nothing queued now is discarded later.
         *
         * Shutdown runs: BeginShutdown() on all three databases, the sessions are kicked
         * and saved, HaltDelayThread() flushes what is left (each finished operation
         * enqueues its callback), and one last ProcessResultQueue() runs those callbacks
         * on the world thread. That last pass is only bounded if nothing new can be
         * queued behind it -- which is what this flag buys. AsyncQuery/AsyncPQuery/
         * DelayQueryHolder answer false from here on; Execute()/PExecute() and the
         * transactions still work, because the save path that runs after this call needs
         * them and they carry no callback to discard.
         */
        void BeginShutdown() { m_shutdown.store(true); }

        /// True once BeginShutdown() has been called (decoupling D7a).
        bool IsShuttingDown() const { return m_shutdown.load(); }

        /**
         * @brief How many operations are waiting in the delay thread's queue.
         *
         * Zero on a database with no delay thread. Printed by `.server database` as the
         * other half of the tick picture: a tick that never waits is not an improvement
         * if the work simply piles up behind the worker.
         */
        size_t GetDelayQueueDepth() const;

        // ---- test seam (mangos_tests only; NO production caller) ----
        //
        // Hands the database a query connection, an async connection and a result queue
        // instead of Initialize()ing one, so the real Query/AsyncPQuery/DelayQueryHolder/
        // ProcessResultQueue/escape_string paths can run in the unit test binary against
        // fakes. The database owns NOTHING it is handed here: the caller keeps and
        // destroys all three, and MUST call DetachTestConnections() before this Database
        // is destroyed, or ~Database()'s StopServer() would delete objects it does not own.
        //
        // The delay thread body is constructed but never started: AsyncQuery() and
        // DelayQueryHolder() queue onto it as usual, and the test drives execution itself
        // with ExecuteQueuedForTest() (see the comment there).

        /// Wire this database to the three objects above, as Initialize() would.
        void AttachTestConnections(SqlConnection* query, SqlConnection* async, SqlResultQueue* results);

        /// Back to the un-initialised state (operator bool() is false again).
        void DetachTestConnections();

        /**
         * @brief Run everything queued on the delay thread, on the CALLING thread.
         *
         * mangos_tests only. This is what the delay thread's loop does once
         * (SqlDelayThread::ProcessRequests): each queued operation runs against the async
         * connection and, for a query, pushes its callback into the result queue --
         * ProcessResultQueue() then invokes it, exactly as the world thread does.
         */
        void ExecuteQueuedForTest();

        /**
         * @brief Leave an attached database in the state HaltDelayThread() leaves a real one.
         *
         * mangos_tests only. The worker and the per-thread transaction slot are gone; the
         * connections and the result queue are still there. That asymmetric state is the one
         * the shutdown drain runs callbacks in, so it is the one the write-refusal test has to
         * reproduce — the seam's body was never started, so `HaltDelayThread()` itself returns
         * early on it and cannot be used.
         */
        void HaltDelayThreadForTest();

    protected:
        /**
         * @brief
         *
         */
        Database() :
            m_TransStorage(NULL),m_nQueryConnPoolSize(1), m_pAsyncConn(NULL), m_pResultQueue(NULL),
            m_threadBody(NULL), m_delayThread(NULL), m_bAllowAsyncTransactions(false),
            m_shutdown(false), m_iStmtIndex(-1), m_logSQL(false), m_pingIntervallms(0)
        {
            m_nQueryCounter = -1;
        }

        /**
         * @brief
         *
         */
        void StopServer();

        /**
         * @brief factory method to create SqlConnection objects
         *
         * @return SqlConnection
         */
        virtual SqlConnection* CreateConnection() = 0;
        /**
         * @brief factory method to create SqlDelayThread objects
         *
         * @return SqlDelayThread
         */
        virtual SqlDelayThread* CreateDelayThread();

        /**
         * @brief
         *
         */
        class TransHelper
        {
            public:
                /**
                 * @brief
                 *
                 */
                TransHelper() : m_pTrans(NULL) {}
                /**
                 * @brief
                 *
                 */
                ~TransHelper();

                /**
                 * @brief initializes new SqlTransaction object
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* init();
                /**
                 * @brief gets pointer on current transaction object. Returns NULL if transaction was not initiated
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* get() const { return m_pTrans; }

                /**
                 * @brief detaches SqlTransaction object allocated by init() function
                 *
                 * next call to get() function will return NULL!
                 * do not forget to destroy obtained SqlTransaction object!
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* detach();
                /**
                 * @brief destroyes SqlTransaction allocated by init() function
                 *
                 */
                void reset();

            private:
                SqlTransaction* m_pTrans; /**< TODO */
        };

        /**
         * @brief per-thread based storage for SqlTransaction object initialization - no locking is required
         *
         */
        typedef MaNGOS::ThreadLocalStore<Database::TransHelper> DBTransHelperTSS;
        Database::DBTransHelperTSS *m_TransStorage; /**< TODO */

        ///< DB connections
        /**
         * @brief Is the deferred-write machinery there at all?
         *
         * The async connection, the per-thread transaction slot and the delay thread body are
         * created together (`Initialize()` -> `InitDelayThread()`) and destroyed together
         * (`HaltDelayThread()`), and every write path needs all three: the slot to see whether
         * a transaction is open, the body to queue onto, the connection to run on.
         *
         * Between the halts and the process's exit the connection is still alive while the
         * other two are NULL, and D7a's shutdown drain now runs callbacks in exactly that
         * window — a callback that writes (`_UpdateRealmCharCount` reaching `PExecute`) would
         * have dereferenced a NULL `m_TransStorage`. So the D1 null-guard treatment has to
         * cover all three, not only the connection. A write refused here is a write the old
         * code discarded unrun anyway, because nothing was left to execute it.
         *
         * @return bool true when a write may still be accepted
         */
        bool CanDelayWork() const { return m_pAsyncConn && m_TransStorage && m_threadBody; }

        /**
         * @brief round-robin connection selection
         *
         * @param sql the statement about to run, for the tick guard's log line only
         * @return SqlConnection
         */
        SqlConnection* getQueryConnection(const char* sql = "");
        /**
         * @brief for now return one single connection for async requests
         *
         * @return SqlConnection
         */
        SqlConnection* getAsyncConnection() const { return m_pAsyncConn; }

        friend class SqlStatement;
        // PREPARED STATEMENT API
        /**
         * @brief query function for prepared statements
         *
         * @param id
         * @param params
         * @return bool
         */
        bool ExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);
        /**
         * @brief
         *
         * @param id
         * @param params
         * @return bool
         */
        bool DirectExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);

        // connection helper counters
        int m_nQueryConnPoolSize;                               /**< current size of query connection pool */
        std::atomic<long> m_nQueryCounter;  /**< counter for connection selection */

        /**
         * @brief lets use pool of connections for sync queries
         *
         */
        typedef std::vector< SqlConnection* > SqlConnectionContainer;
        SqlConnectionContainer m_pQueryConnections; /**< TODO */

        // only one single DB connection for transactions
        SqlConnection* m_pAsyncConn; /**< TODO */

        SqlResultQueue*     m_pResultQueue;                 /**< Transaction queues from diff. threads */
        SqlDelayThread*     m_threadBody;                   /**< Pointer to delay sql executer (owned by m_delayThread) */
        MaNGOS::Thread*  m_delayThread;                  /**< Pointer to executer thread */

        bool m_bAllowAsyncTransactions;                     /**< flag which specifies if async transactions are enabled */

        std::atomic<bool> m_shutdown;                       /**< BeginShutdown(): no new async work is accepted */

        // PREPARED STATEMENT REGISTRY
        /**
         * @brief
         *
         */
        typedef std::mutex LOCK_TYPE;
        /**
         * @brief
         *
         */
        typedef std::lock_guard<LOCK_TYPE> LOCK_GUARD;

        mutable LOCK_TYPE m_stmtGuard; /**< TODO */

        /**
         * @brief
         *
         */
        typedef std::unordered_map<std::string, int> PreparedStmtRegistry;
        PreparedStmtRegistry m_stmtRegistry;                ///< /**< TODO */

        int m_iStmtIndex; /**< TODO */

    private:

        bool m_logSQL; /**< TODO */
        std::string m_logsDir; /**< TODO */
        uint32 m_pingIntervallms; /**< TODO */
};

/**
 * @brief RAII pairing of ThreadStart() and ThreadEnd() for a worker thread.
 *
 * The MySQL client library keeps per-thread state, and every thread that issues
 * a query on a connection it did not create itself must register with it first
 * and release that state on the way out. Database declares ThreadStart()/
 * ThreadEnd() as that contract and DatabaseMysql implements them; a thread that
 * skips them corrupts or leaks the library's thread-local data, which surfaces
 * far from the cause and only under load.
 *
 * Use this rather than calling the pair by hand: it survives early returns and
 * exceptions, and it keeps the backend-specific call behind the interface. A
 * null database is tolerated, so the guard can sit in a thread body that may run
 * without one.
 */
class DbThreadGuard
{
    public:

        explicit DbThreadGuard(Database* db) : m_db(db)
        {
            if (m_db)
            {
                m_db->ThreadStart();
            }
        }

        ~DbThreadGuard()
        {
            if (m_db)
            {
                m_db->ThreadEnd();
            }
        }

        DbThreadGuard(const DbThreadGuard&) = delete;
        DbThreadGuard& operator=(const DbThreadGuard&) = delete;

    private:

        Database* m_db;
};

#endif
