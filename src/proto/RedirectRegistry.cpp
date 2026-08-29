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

#include "RedirectRegistry.h"

#include "Log/Log.h"

#include <algorithm>

namespace proto
{
    RedirectRegistry::RedirectRegistry(std::chrono::milliseconds timeout)
        : m_timeout(timeout)
    {
    }

    void RedirectRegistry::ExpireLocked(Clock::time_point now)
    {
        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [now](const Entry& entry) { return entry.deadline <= now; }),
            m_entries.end());
    }

    bool RedirectRegistry::Open(const RedirectTicket& ticket)
    {
        const Clock::time_point now = Clock::now();

        std::lock_guard<std::mutex> lock(m_lock);
        ExpireLocked(now);

        for (const Entry& entry : m_entries)
        {
            if (entry.ticket.clientAddress == ticket.clientAddress)
            {
                return false;
            }
        }

        Entry entry;
        entry.ticket   = ticket;
        entry.deadline = now + m_timeout;
        m_entries.push_back(entry);

        return true;
    }

    bool RedirectRegistry::Claim(const std::string& clientAddress, RedirectTicket& out)
    {
        const Clock::time_point now = Clock::now();

        std::lock_guard<std::mutex> lock(m_lock);
        ExpireLocked(now);

        for (auto entry = m_entries.begin(); entry != m_entries.end(); ++entry)
        {
            if (entry->ticket.clientAddress == clientAddress)
            {
                out = entry->ticket;
                m_entries.erase(entry);
                return true;
            }
        }

        return false;
    }

    void RedirectRegistry::Cancel(SessionId session)
    {
        std::lock_guard<std::mutex> lock(m_lock);

        m_entries.erase(
            std::remove_if(m_entries.begin(), m_entries.end(),
                [session](const Entry& entry) { return entry.ticket.session == session; }),
            m_entries.end());
    }

    size_t RedirectRegistry::ExpireStale()
    {
        const Clock::time_point now = Clock::now();

        std::lock_guard<std::mutex> lock(m_lock);

        const size_t before = m_entries.size();
        ExpireLocked(now);
        return before - m_entries.size();
    }

    uint32 RedirectRegistry::InFlightCount() const
    {
        std::lock_guard<std::mutex> lock(m_lock);
        return uint32(m_entries.size());
    }
}
