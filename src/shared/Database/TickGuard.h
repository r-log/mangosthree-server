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

    bool Active();                  ///< true when the CALLING thread holds a Scope
    uint32 Violations();            ///< process-wide count since start (or since the last reset)
    void ResetViolations();         ///< back to zero (the tests, and a future `.server database reset`)
    void Violation(char const* sql);///< counts one acquisition; the first sixteen are logged
}

#endif
