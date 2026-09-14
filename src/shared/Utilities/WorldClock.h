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

#ifndef MANGOS_H_WORLDCLOCK
#define MANGOS_H_WORLDCLOCK

#include "Platform/Define.h"

#include <chrono>
#include <ctime>

/**
 * @brief The process clock: the single source of "now" behind getMSTime() (movement P0-D).
 *
 * In real mode NowMs() is the steady clock since the process started. While the GM
 * harness steps the world, NowMs() is a counter only Step() moves, so a scenario's
 * timing does not depend on how fast the machine runs it. Leaving stepped mode keeps
 * an offset, so the clock never runs backwards; it then stays ahead of real time by the
 * run's virtual length, which every consumer tolerates since all of them are relative.
 * The world thread is the only writer; any thread may read.
 */
namespace WorldClock
{
    /// Milliseconds since the process started: the steady clock, or the stepped counter while a run steps the world.
    uint32 NowMs();
    /// Seconds since the epoch from the same source: the wall clock at start plus NowMs().
    time_t NowUnix();
    /// True while the harness steps the world.
    bool IsStepped();
    /// Enter stepped mode: the counter starts at the current NowMs() and only Step() moves it.
    void EnterStepped();
    /// Advance the stepped counter.
    void Step(uint32 ms);
    /// Leave stepped mode: real time resumes from the counter's value (an offset keeps it from running backwards).
    void LeaveStepped();
    /// The lead the clock keeps over real time after stepped runs, in milliseconds.
    uint32 OffsetMs();
    /// The steady-clock point the process started at (the old GetApplicationStartTime()).
    std::chrono::steady_clock::time_point StartPoint();
}

#endif
