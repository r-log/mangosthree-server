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

#ifndef MANGOS_TICKGUARD_H
#define MANGOS_TICKGUARD_H

#include "Platform/Define.h"

/**
 * Which threads may not wait on MySQL (decoupling D7): the world thread inside
 * World::Update, and a map-update worker while it is running one map's tick. A
 * Scope marks such a stretch on the CALLING thread -- not process-wide, because
 * the network threads and the delay thread legitimately acquire a connection
 * while the tick runs, and a process-wide flag would count their work as the
 * tick's.
 *
 * The database layer's acquisition points (Database.cpp: the pooled query
 * connection, the direct-execute family, Ping, CommitTransactionChecked's wait,
 * escape_string) ask Active() and, when it is true, count a Violation. D7a only
 * counts and logs: the count is the instrument the conversion PRs are measured
 * against, and MANGOS_STRICT_TICK (off by default) is what turns it into an
 * assert once a family is meant to be clean.
 *
 * A violation is not a bug by itself -- a start-up load or a console command
 * running on the world thread outside World::Update is not counted at all, and
 * a converted handler is expected to reach zero. What it is, is the only honest
 * record of what the tick actually waits for, including the indirect chains a
 * static scan cannot see.
 */
namespace TickGuard
{
    /// RAII: held by World::Update over its whole body, and by each MapUpdater
    /// worker around one map's Update(). Nested scopes are counted by depth, so
    /// a map update nested inside another (a transport deck) stays marked.
    struct Scope
    {
        Scope();
        ~Scope();

        Scope(Scope const&) = delete;
        Scope& operator=(Scope const&) = delete;
    };

    /// RAII, thread-local exactly like Scope, and entered at ONE site in the tree:
    /// ChatHandler::ExecuteCommand, around a `.reload <table>` command's handler
    /// (decoupling D7h).
    ///
    /// Every `.reload` re-runs a start-up loader synchronously, on the world thread,
    /// inside World::Update. That is what the command IS: an administrator asking a
    /// running server to stall while it re-reads a table. Those acquisitions are real
    /// and are still counted -- in a counter of their own, which `.server database`
    /// prints on its own line -- but they do not assert under MANGOS_STRICT_TICK,
    /// because the tick's contract is about what the world does by itself, not about
    /// what an administrator deliberately asks it to wait for.
    ///
    /// A scope is code, not a comment, and its worth depends on staying at one site:
    /// src/tests/CheckSyncDb.cmake fails the build if `TickGuard::AdminScope` appears
    /// anywhere but that one line.
    ///
    /// `enter` is the dispatcher's test, so the one site can be an unconditional
    /// declaration: a command that is not a reload constructs a scope that does
    /// nothing.
    struct AdminScope
    {
        explicit AdminScope(bool enter);
        ~AdminScope();

        AdminScope(AdminScope const&) = delete;
        AdminScope& operator=(AdminScope const&) = delete;

    private:
        bool m_entered;
    };

    bool Active();                  ///< true when the CALLING thread holds a Scope
    bool AdminActive();             ///< true when the CALLING thread holds an entered AdminScope
    uint32 Violations();            ///< process-wide count since start (or since the last reset)
    uint32 AdminViolations();       ///< the same, for acquisitions made inside an AdminScope
    void ResetViolations();         ///< both counters back to zero (the tests, and a future `.server database reset`)
    void Violation(char const* sql);///< counts one acquisition; the first sixteen are logged

    /// Whether this build was configured with -DMANGOS_STRICT_TICK=ON, i.e. whether
    /// Violation() aborts the process instead of only counting. The option reaches
    /// every consumer through shared_db's PUBLIC compile definition, so this answers
    /// for the guard's own translation unit as well as for the caller's.
    ///
    /// It exists so that code which must behave differently under an armed guard can
    /// say so in C++ rather than in a #ifdef at each site -- the test suite's cases
    /// that deliberately provoke a violation are the first such callers, since under
    /// a strict build they would abort the whole binary.
    constexpr bool Strict()
    {
#ifdef MANGOS_STRICT_TICK
        return true;
#else
        return false;
#endif
    }
}

#endif
