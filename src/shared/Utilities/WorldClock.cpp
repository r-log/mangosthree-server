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

#include "WorldClock.h"

#include <atomic>

namespace
{
    std::chrono::steady_clock::time_point const& StartPointStorage()
    {
        static const std::chrono::steady_clock::time_point point = std::chrono::steady_clock::now();
        return point;
    }

    time_t StartUnix()
    {
        static const time_t start = std::time(nullptr);
        return start;
    }

    uint32 RealMs()
    {
        using namespace std::chrono;
        return uint32(duration_cast<milliseconds>(steady_clock::now() - StartPointStorage()).count());
    }

    std::atomic<bool>   g_stepped{false};
    std::atomic<uint32> g_counterMs{0};
    std::atomic<uint32> g_offsetMs{0};
}

namespace WorldClock
{
    uint32 NowMs()
    {
        if (g_stepped.load(std::memory_order_acquire))
        {
            return g_counterMs.load(std::memory_order_acquire);
        }
        return RealMs() + g_offsetMs.load(std::memory_order_acquire);
    }

    time_t NowUnix()
    {
        return StartUnix() + time_t(NowMs() / 1000);
    }

    bool IsStepped()
    {
        return g_stepped.load(std::memory_order_acquire);
    }

    void EnterStepped()
    {
        g_counterMs.store(NowMs(), std::memory_order_release);
        g_stepped.store(true, std::memory_order_release);
    }

    void Step(uint32 ms)
    {
        g_counterMs.fetch_add(ms, std::memory_order_acq_rel);
    }

    void LeaveStepped()
    {
        if (!g_stepped.load(std::memory_order_acquire))
        {
            return;   // already real: the offset stands, the counter is stale
        }

        const uint32 counter = g_counterMs.load(std::memory_order_acquire);
        const uint32 real = RealMs();
        g_offsetMs.store(counter > real ? counter - real : 0, std::memory_order_release);
        g_stepped.store(false, std::memory_order_release);
    }

    uint32 OffsetMs()
    {
        return g_offsetMs.load(std::memory_order_acquire);
    }

    std::chrono::steady_clock::time_point StartPoint()
    {
        return StartPointStorage();
    }
}
