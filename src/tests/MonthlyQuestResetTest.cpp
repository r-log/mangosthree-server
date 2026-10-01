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

#include "TestHarness.h"
#include "World.h"
#include "Utilities/Util.h"

#include <ctime>

namespace
{
    time_t LocalTime(int year, int month, int day, int hour, int minute, int second)
    {
        std::tm t{};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = second;
        t.tm_isdst = -1;
        return mktime(&t);
    }

    bool IsLocalMidnightOn(time_t when, int year, int month, int day)
    {
        const std::tm t = safe_localtime(when);
        return t.tm_year == year - 1900 && t.tm_mon == month - 1 && t.tm_mday == day
            && t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0;
    }

    /// Ticks the reset over [from, to) the way World::Update does; the next reset is computed from computeNow(tick).
    template<class ComputeNow>
    int CountFirings(time_t& stored, time_t from, time_t to, ComputeNow computeNow)
    {
        int firings = 0;
        for (time_t tick = from; tick < to; ++tick)
        {
            if (tick > stored)
            {
                ++firings;
                stored = NextMonthlyQuestReset(computeNow(tick), stored, false);
            }
        }
        return firings;
    }
}

TEST(MonthlyQuestReset_TheNextResetIsMidnightOnTheFirstOfTheFollowingMonth)
{
    struct Row { int y, mo, d, h, mi, s; int ny, nmo; };
    const Row rows[] =
    {
        { 2026,  1, 31, 23, 59, 59, 2026, 2 },   // month end
        { 2026,  1,  1,  0,  0,  0, 2026, 2 },   // exactly a reset
        { 2026,  2, 28, 23, 59, 59, 2026, 3 },   // February, common year
        { 2028,  2, 29, 12,  0,  0, 2028, 3 },   // February, leap year
        { 2028,  2,  1,  0,  0,  1, 2028, 3 },
        { 2026, 12, 31, 23, 59, 59, 2027, 1 },   // year end
        { 2027, 12,  1,  0,  0,  0, 2028, 1 },
        { 2026,  6, 30, 23, 59, 59, 2026, 7 },
        { 2027,  7, 15,  6, 30,  0, 2027, 8 },
    };
    for (const Row& r : rows)
    {
        const time_t now = LocalTime(r.y, r.mo, r.d, r.h, r.mi, r.s);
        const time_t next = NextMonthlyQuestReset(now, 0, false);
        CHECK(IsLocalMidnightOn(next, r.ny, r.nmo, 1));
        CHECK(next > now);
    }
}

TEST(MonthlyQuestReset_TheNextResetIsAlwaysAfterNow)
{
    const time_t first = LocalTime(2026, 1, 1, 0, 0, 0);
    const time_t last = LocalTime(2029, 1, 1, 0, 0, 0);
    int checked = 0;
    for (time_t now = first; now < last; now += 1800)
    {
        for (time_t probe : { now - 1, now, now + 1 })
        {
            const time_t next = NextMonthlyQuestReset(probe, probe - 1, false);
            CHECK(next > probe);
            CHECK(next - probe <= 31 * DAY + HOUR);
            ++checked;
        }
    }
    CHECK(checked > 150000);
}

TEST(MonthlyQuestReset_AtStartupAnEarlierStoredResetIsKept)
{
    const time_t now = LocalTime(2027, 1, 10, 12, 0, 0);
    const time_t next = LocalTime(2027, 2, 1, 0, 0, 0);
    const time_t missed = LocalTime(2027, 1, 1, 0, 0, 0);
    const time_t later = LocalTime(2027, 3, 1, 0, 0, 0);

    CHECK_EQ(NextMonthlyQuestReset(now, missed, true), missed);
    CHECK_EQ(NextMonthlyQuestReset(now, next, true), next);
    CHECK_EQ(NextMonthlyQuestReset(now, later, true), next);
    CHECK_EQ(NextMonthlyQuestReset(now, missed, false), next);
}

TEST(MonthlyQuestReset_FiresOnceWhenTheGameClockRunsAheadOfTheWallClock)
{
    const time_t boundary = LocalTime(2027, 2, 1, 0, 0, 0);
    const time_t lead = 1782;

    time_t stored = boundary;
    const int firings = CountFirings(stored, boundary - HOUR, boundary + HOUR, [](time_t gameNow) { return gameNow; });
    CHECK_EQ(firings, 1);
    CHECK(IsLocalMidnightOn(stored, 2027, 3, 1));

    time_t storedFromWallClock = boundary;
    const int wallClockFirings = CountFirings(storedFromWallClock, boundary - HOUR, boundary + HOUR,
        [lead](time_t gameNow) { return gameNow - lead; });
    CHECK_EQ(wallClockFirings, int(lead));
}

TEST(MonthlyQuestReset_FiresOnceAtTheBoundaryWithNoLead)
{
    const time_t boundary = LocalTime(2026, 12, 1, 0, 0, 0);
    time_t stored = boundary;
    const int firings = CountFirings(stored, boundary - HOUR, boundary + HOUR, [](time_t now) { return now; });
    CHECK_EQ(firings, 1);
    CHECK(IsLocalMidnightOn(stored, 2027, 1, 1));
}
