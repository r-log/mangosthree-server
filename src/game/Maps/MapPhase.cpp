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

#include "MapPhase.h"
#include "Log.h"

#include <atomic>

namespace
{
    std::atomic<bool>   s_active(false);
    std::atomic<uint32> s_violations(0);
    thread_local MapPhase::Scope const* t_innermost = NULL;
}

namespace MapPhase
{
    void Begin() { s_active.store(true); }
    void End() { s_active.store(false); }
    bool Active() { return s_active.load(); }
    bool UpdatesAMap() { return t_innermost != NULL; }
    uint32 Violations() { return s_violations.load(); }

    bool Owns(Map const* map)
    {
        if (!s_active.load())
        {
            return true;
        }
        for (Scope const* scope = t_innermost; scope != NULL; scope = scope->m_outer)
        {
            if (scope->m_map == map)
            {
                return true;
            }
        }
        return false;
    }

    bool OwnsUnitOn(Map const* map, Map const* sailed)
    {
        return Owns(map) || (sailed != NULL && Owns(sailed));
    }

    Scope::Scope(Map const* map) : m_map(map), m_outer(t_innermost)
    {
        t_innermost = this;
    }

    Scope::~Scope()
    {
        t_innermost = m_outer;
    }

    void Violation(char const* subject, char const* unit)
    {
        const uint32 n = ++s_violations;
        if (n <= 10)
        {
            sLog.outError("MapPhase: %s of %s touched from outside its map's update (violation %u)", subject, unit, n);
        }
    }
}
