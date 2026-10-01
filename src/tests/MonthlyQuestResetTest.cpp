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

#include <cstdlib>
#include <ctime>
#include <set>
#include <string>

namespace
{
    const char* const kEuropeanZone = "CET-1CEST,M3.5.0,M10.5.0/3";
    const char* const kUsEasternZone = "EST5EDT,M3.2.0,M11.1.0";

    /// The Windows CRT ignores the rule part of TZ and applies the US change dates to every zone.
#if defined(_WIN32)
    const bool kCrtAppliesUsDates = true;
#else
    const bool kCrtAppliesUsDates = false;
#endif

    /// Sets TZ for a scope and restores the previous value, or its absence, afterwards.
    class PinnedZone
    {
    public:
        explicit PinnedZone(const char* zone)
        {
            const char* existing = std::getenv("TZ");
            m_hadPrevious = existing != nullptr;
            if (m_hadPrevious)
            {
                m_previous = existing;
            }
            Set(zone);
        }

        ~PinnedZone()
        {
            Set(m_hadPrevious ? m_previous.c_str() : nullptr);
        }

        PinnedZone(const PinnedZone&) = delete;
        PinnedZone& operator=(const PinnedZone&) = delete;

    private:
        static void Set(const char* zone)
        {
#if defined(_WIN32)
            _putenv_s("TZ", zone ? zone : "");
            _tzset();
#else
            if (zone)
            {
                setenv("TZ", zone, 1);
            }
            else
            {
                unsetenv("TZ");
            }
            tzset();
#endif
        }

        bool m_hadPrevious = false;
        std::string m_previous;
    };

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

    bool IsLocalMidnightOnTheFirst(time_t when)
    {
        const std::tm t = safe_localtime(when);
        return t.tm_mday == 1 && t.tm_hour == 0 && t.tm_min == 0 && t.tm_sec == 0;
    }

    bool OnDaylightTime(time_t when)
    {
        return safe_localtime(when).tm_isdst > 0;
    }

    /// Ticks the reset over [from, to) the way World::Update does; the next reset is computed from computeNow(tick).
    template<class ComputeNow>
    int CountFirings(time_t& stored, time_t from, time_t to, ComputeNow computeNow, time_t* firstFiring = nullptr)
    {
        int firings = 0;
        for (time_t tick = from; tick < to; ++tick)
        {
            if (tick > stored)
            {
                if (firings == 0 && firstFiring)
                {
                    *firstFiring = tick;
                }
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

TEST(MonthlyQuestReset_LandsOnLocalMidnightWhenTheFirstIsOnTheOtherSideOfADaylightSavingChange)
{
    struct Row { const char* zone; int y, mo, d; int ny, nmo; bool straddlesPosix, straddlesWindows; };
    const Row rows[] =
    {
        { kEuropeanZone,  2027,  3, 10, 2027,  4, true,  true  },   // spring, both rule sets
        { kEuropeanZone,  2026, 10,  1, 2026, 11, true,  false },   // autumn, European dates
        { kEuropeanZone,  2027, 11,  2, 2027, 12, false, true  },   // autumn, US dates
        { kUsEasternZone, 2027,  3, 10, 2027,  4, true,  true  },
        { kUsEasternZone, 2026, 10,  1, 2026, 11, false, false },
        { kUsEasternZone, 2027, 11,  2, 2027, 12, true,  true  },
    };
    for (const Row& r : rows)
    {
        const PinnedZone zone(r.zone);
        const time_t now = LocalTime(r.y, r.mo, r.d, 12, 0, 0);
        const time_t midnight = LocalTime(r.ny, r.nmo, 1, 0, 0, 0);

        const bool straddles = kCrtAppliesUsDates ? r.straddlesWindows : r.straddlesPosix;
        CHECK(IsLocalMidnightOn(midnight, r.ny, r.nmo, 1));
        CHECK_EQ(OnDaylightTime(now) != OnDaylightTime(midnight), straddles);

        const time_t next = NextMonthlyQuestReset(now, 0, false);
        CHECK(IsLocalMidnightOn(next, r.ny, r.nmo, 1));
        CHECK_EQ(next, midnight);
        CHECK(next > now);
    }
}

TEST(MonthlyQuestReset_TheNextResetIsLocalMidnightAndAfterNowAllYearInADaylightSavingZone)
{
    const PinnedZone zone(kEuropeanZone);
    const time_t first = LocalTime(2026, 1, 1, 0, 0, 0);
    const time_t last = LocalTime(2029, 1, 1, 0, 0, 0);
    int checked = 0;
    int notAfterNow = 0;
    int tooFarAhead = 0;
    int notMidnightOnTheFirst = 0;
    int notTheFollowingMonth = 0;
    std::set<int> straddledMonths;
    for (time_t now = first; now < last; now += 1800)
    {
        for (time_t probe : { now - 1, now, now + 1 })
        {
            const time_t next = NextMonthlyQuestReset(probe, probe - 1, false);
            const std::tm probeTm = safe_localtime(probe);
            const std::tm nextTm = safe_localtime(next);
            notAfterNow += next <= probe;
            tooFarAhead += next - probe > 31 * DAY + HOUR;
            notMidnightOnTheFirst += !IsLocalMidnightOnTheFirst(next);
            const int monthsAhead = (nextTm.tm_year * 12 + nextTm.tm_mon) - (probeTm.tm_year * 12 + probeTm.tm_mon);
            notTheFollowingMonth += monthsAhead != 1;
            if (OnDaylightTime(probe) != OnDaylightTime(next))
            {
                straddledMonths.insert(nextTm.tm_year * 12 + nextTm.tm_mon);
            }
            ++checked;
        }
    }
    CHECK(checked > 150000);
    CHECK_EQ(notAfterNow, 0);
    CHECK_EQ(tooFarAhead, 0);
    CHECK_EQ(notMidnightOnTheFirst, 0);
    CHECK_EQ(notTheFollowingMonth, 0);
    CHECK_EQ(straddledMonths.size(), size_t(6));             // a spring and an autumn month in each of the three years
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

TEST(MonthlyQuestReset_AStoredResetAnHourOffMidnightIsMovedToMidnight)
{
    struct Row { const char* zone; int y, mo; };
    const Row rows[] =
    {
        { kEuropeanZone,  2026, 11 },
        { kEuropeanZone,  2027,  4 },
        { kEuropeanZone,  2027, 12 },
        { kUsEasternZone, 2027,  4 },
        { kUsEasternZone, 2027, 12 },
    };
    for (const Row& r : rows)
    {
        const PinnedZone zone(r.zone);
        const time_t midnight = LocalTime(r.y, r.mo, 1, 0, 0, 0);
        const time_t early = midnight - HOUR;
        const time_t late = midnight + HOUR;

        const std::tm earlyTm = safe_localtime(early);
        const std::tm lateTm = safe_localtime(late);
        CHECK(IsLocalMidnightOn(midnight, r.y, r.mo, 1));
        CHECK(earlyTm.tm_mday != 1 && earlyTm.tm_hour == 23);
        CHECK(lateTm.tm_mday == 1 && lateTm.tm_hour == 1);

        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(early), midnight);
        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(late), midnight);
        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight), midnight);
        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(0), time_t(0));
    }
}

TEST(MonthlyQuestReset_AStoredResetHoursLateOrDaysEarlyStaysInItsMonth)
{
    const PinnedZone zone("UTC0");
    const time_t midnight = LocalTime(2026, 11, 1, 0, 0, 0);
    CHECK(IsLocalMidnightOn(midnight, 2026, 11, 1));

    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight + 5 * HOUR), midnight);
    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight + 12 * HOUR - 1), midnight);
    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight - 6 * HOUR), midnight);
    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight - 20 * DAY), midnight);
    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(midnight), midnight);
}

TEST(MonthlyQuestReset_TheResetStoredForNovemberOnACentralEuropeanServerIsMovedToMidnight)
{
    const PinnedZone zone(kEuropeanZone);
    const time_t stored = 1793484000;                       // 2026-10-31 22:00 UTC
    const time_t midnight = LocalTime(2026, 11, 1, 0, 0, 0);

    // Under US change dates the stored value is already midnight on November 1st in this zone.
    if (kCrtAppliesUsDates)
    {
        CHECK(IsLocalMidnightOn(stored, 2026, 11, 1));
        CHECK_EQ(midnight, stored);
        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(stored), stored);
    }
    else
    {
        CHECK(IsLocalMidnightOn(stored + HOUR, 2026, 11, 1));
        CHECK_EQ(midnight, time_t(1793487600));
        CHECK_EQ(MonthlyQuestResetOnLocalMidnight(stored), midnight);
    }

    // at startup half an hour before midnight: the reset fires once, at midnight
    const time_t boot = midnight - HOUR / 2;
    time_t next = NextMonthlyQuestReset(boot, MonthlyQuestResetOnLocalMidnight(stored), true);
    CHECK_EQ(next, midnight);
    time_t firedAt = 0;
    const int firings = CountFirings(next, boot, midnight + 2 * HOUR, [](time_t gameNow) { return gameNow; }, &firedAt);
    CHECK_EQ(firings, 1);
    CHECK_EQ(firedAt, midnight + 1);
    CHECK(IsLocalMidnightOn(next, 2026, 12, 1));
}

// The Windows C runtime applies US change rules to every zone string, so no zone pinned here reaches this there.
#if !defined(_WIN32)
TEST(MonthlyQuestReset_AMidnightThatDoesNotExistResolvesToTheFirstMomentOfTheFirst)
{
    // daylight time one hour behind standard time: October 1st starts at 01:00
    const PinnedZone zone("AAA-3BBB-2,J91/0,J274/0");
    const time_t now = LocalTime(2026, 9, 15, 12, 0, 0);

    std::tm raw{};
    raw.tm_year = 2026 - 1900;
    raw.tm_mon = 9;
    raw.tm_mday = 1;
    raw.tm_isdst = -1;
    const time_t rawMidnight = mktime(&raw);
    REQUIRE(safe_localtime(rawMidnight).tm_mday == 30);

    const time_t next = NextMonthlyQuestReset(now, 0, false);
    const std::tm nextTm = safe_localtime(next);
    CHECK(next > now);
    CHECK(nextTm.tm_mon == 9 && nextTm.tm_mday == 1 && nextTm.tm_hour == 1 && nextTm.tm_min == 0 && nextTm.tm_sec == 0);
    CHECK(safe_localtime(next - 1).tm_mday == 30);
    CHECK_EQ(MonthlyQuestResetOnLocalMidnight(next), next);
}
#endif

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

TEST(MonthlyQuestReset_FiresOnceAtLocalMidnightAcrossADaylightSavingChange)
{
    struct Row { const char* zone; int y, mo, d; int ny, nmo; int fy, fmo; };
    const Row rows[] =
    {
        { kEuropeanZone,  2027,  3, 10, 2027,  4, 2027, 5 },   // spring, both rule sets
        { kUsEasternZone, 2027, 11,  2, 2027, 12, 2028, 1 },   // autumn, both rule sets
    };
    for (const Row& r : rows)
    {
        const PinnedZone zone(r.zone);
        const time_t computedAt = LocalTime(r.y, r.mo, r.d, 12, 0, 0);
        const time_t midnight = LocalTime(r.ny, r.nmo, 1, 0, 0, 0);
        CHECK(OnDaylightTime(computedAt) != OnDaylightTime(midnight));

        const time_t lead = 1782;
        time_t stored = NextMonthlyQuestReset(computedAt, 0, false);
        CHECK_EQ(stored, midnight);
        time_t firedAt = 0;
        const int firings = CountFirings(stored, midnight - 3 * HOUR, midnight + 3 * HOUR,
            [](time_t gameNow) { return gameNow; }, &firedAt);
        CHECK_EQ(firings, 1);
        CHECK_EQ(firedAt, midnight + 1);
        CHECK(IsLocalMidnightOn(stored, r.fy, r.fmo, 1));

        time_t storedFromWallClock = midnight;
        const int wallClockFirings = CountFirings(storedFromWallClock, midnight - 3 * HOUR, midnight + 3 * HOUR,
            [lead](time_t gameNow) { return gameNow - lead; });
        CHECK_EQ(wallClockFirings, int(lead));
    }
}

TEST(MonthlyQuestReset_FiresOnceAtTheBoundaryWithNoLead)
{
    const time_t boundary = LocalTime(2026, 12, 1, 0, 0, 0);
    time_t stored = boundary;
    const int firings = CountFirings(stored, boundary - HOUR, boundary + HOUR, [](time_t now) { return now; });
    CHECK_EQ(firings, 1);
    CHECK(IsLocalMidnightOn(stored, 2027, 1, 1));
}
