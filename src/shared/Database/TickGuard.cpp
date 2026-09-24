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

#include "TickGuard.h"
#include "Log.h"
#include "Utilities/Errors.h"

#include <atomic>
#include <cstring>

namespace
{
    /// Per thread, and a depth rather than a flag: World::Update holds one for its
    /// whole body, and a map update nested inside another map's update (a transport
    /// deck) opens a second one on the same thread.
    thread_local uint32 t_depth = 0;

    /// Process-wide: the world thread and every map-update worker add to the same
    /// number, which is what `.server database` prints.
    std::atomic<uint32> s_violations(0);

    /// How many of them carry their SQL into the log. Enough to name the offenders
    /// of one run; past that the count is the record.
    const uint32 LOGGED_VIOLATIONS = 16;

    /// Of the SQL, not the whole statement: a 32 KB query in an error line helps
    /// nobody, and the prefix is what identifies the site.
    const size_t LOGGED_SQL_CHARS = 80;
}

namespace TickGuard
{
    Scope::Scope()
    {
        ++t_depth;
    }

    Scope::~Scope()
    {
        --t_depth;
    }

    bool Active()
    {
        return t_depth > 0;
    }

    uint32 Violations()
    {
        return s_violations.load();
    }

    void ResetViolations()
    {
        s_violations.store(0);
    }

    void Violation(char const* sql)
    {
        const uint32 n = ++s_violations;

        if (n <= LOGGED_VIOLATIONS)
        {
            char prefix[LOGGED_SQL_CHARS + 1];
            const char* from = sql ? sql : "";
            size_t i = 0;
            for (; i < LOGGED_SQL_CHARS && from[i]; ++i)
            {
                prefix[i] = from[i];
            }
            prefix[i] = '\0';

            sLog.outError("TickGuard: synchronous database acquisition on a tick thread "
                          "(violation %u): %s", n, prefix);
        }

#ifdef MANGOS_STRICT_TICK
        // Logged first, so the assert's own output is not the only thing the operator sees.
        MANGOS_ASSERT(false && "synchronous database acquisition on a tick thread");
#endif
    }
}
