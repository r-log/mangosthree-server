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

#include "Utilities/Util.h"

#include <cstdlib>
#include <string>

// CalendarHandler.cpp's create/update/copy handlers compared a client-sent
// packed date bitfield directly against GameTime::GetGameTime(), a unix
// timestamp, before ever unpacking it with timeBitFieldsToSecs(). The two
// live on wholly different scales -- a packed bitfield tops out under 600
// million, current unix time is past 1.7 billion -- so "is this event in the
// past" always took the past branch, for every date, for every player. The
// handler then returned with no reply at all, which reads to the client as a
// frozen Create button: CalendarCreateEventCreateButton_OnClick waits
// forever for a packet that was never going to arrive.
//
// CalendarHandler.cpp itself cannot be linked into mangos_tests -- it depends
// on Player, ObjectMgr and sWorld, which pull in the whole game library. So
// the fix (abbb8a8c3) moved the conversion AND the "is this in the past"
// comparison out of the three handlers and into CalendarPackedTimeToTimestamp() /
// CalendarPackedTimeIsPast() in shared/Utilities/Util, which this test links
// directly. These cases guard those two functions specifically: they would
// catch a future revert of either function back to comparing the raw packed
// value, or any other change that makes them stop agreeing with the
// behaviour pinned below. They cannot catch a handler that stops calling
// them and reintroduces the raw comparison inline instead -- that class of
// regression has no test coverage without linking `game` into this binary.

namespace
{
    // Client bit layout for CMSG_CALENDAR_ADD_EVENT / UPDATE_EVENT / COPY_EVENT,
    // mirrored by timeBitFieldsToSecs(): minute in bits 0-5, hour in 6-10,
    // weekday in 11-13 (left at 0 here -- mktime recomputes it from the other
    // fields and ignores whatever tm_wday is set to), day in 14-19 as
    // (mday - 1), month in 20-23 as (month - 1), year in 24-28 as (year - 2000).
    uint32 PackClientDate(uint32 year, uint32 month, uint32 day, uint32 hour, uint32 minute)
    {
        return ((year - 2000) << 24) | ((month - 1) << 20) | ((day - 1) << 14) | (hour << 6) | minute;
    }

    // Builds the packed value a client would send for "now, shifted by
    // offsetSeconds", by breaking the shifted instant down into local
    // calendar fields and repacking them -- the same round trip the client
    // itself does when it reads its own clock and time-zone. Deriving from
    // the current time (rather than a literal date) is what keeps the
    // future/past tests below meaningful for as long as this file exists,
    // instead of quietly testing a date that was "in the future" only in
    // 2026. time_t arithmetic handles month/year rollover for free, so no
    // separate normalisation step is needed before the field breakdown.
    uint32 PackClientDateOffsetFromNow(time_t now, time_t offsetSeconds)
    {
        const std::tm t = safe_localtime(now + offsetSeconds);
        return PackClientDate(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min);
    }

    // Saves and restores the TZ environment variable around a scope, including the case where
    // it was not set at all beforehand. Used below to force a non-UTC host timezone for the
    // double-conversion regression test -- see that test for why the zone must be forced rather
    // than left at whatever the host happens to run in. RAII rather than a manual
    // save/set/restore sequence so the previous zone comes back on every exit path (including a
    // CHECK_EQ failure partway through, which reports and carries on rather than throwing).
    class ScopedTimeZone
    {
    public:
        explicit ScopedTimeZone(const char* zone)
        {
            const char* existing = std::getenv("TZ");
            m_hadPrevious = existing != nullptr;
            if (m_hadPrevious)
            {
                m_previous = existing;
            }
            SetTZ(zone);
        }

        ~ScopedTimeZone()
        {
            if (m_hadPrevious)
            {
                SetTZ(m_previous.c_str());
            }
            else
            {
                UnsetTZ();
            }
        }

        ScopedTimeZone(const ScopedTimeZone&) = delete;
        ScopedTimeZone& operator=(const ScopedTimeZone&) = delete;

    private:
        static void SetTZ(const char* zone)
        {
#if defined(_WIN32)
            const std::string assignment = std::string("TZ=") + zone;
            _putenv(assignment.c_str());
            _tzset();
#else
            setenv("TZ", zone, 1);
            tzset();
#endif
        }

        static void UnsetTZ()
        {
#if defined(_WIN32)
            // The Windows CRT has no unsetenv(); assigning an empty value is what _tzset()
            // treats as "TZ not set", falling back to the OS-configured zone.
            _putenv("TZ=");
            _tzset();
#else
            unsetenv("TZ");
            tzset();
#endif
        }

        bool m_hadPrevious = false;
        std::string m_previous;
    };
}

TEST(CalendarTime_packed_bitfield_round_trips_to_the_calendar_date)
{
    // The exact packed value the live bug produced: an event dated
    // 2026-09-03 at 12:00, a few days out from the user's create attempt.
    // Pinning the formula's output to this literal ties the test to the
    // actual incident rather than to an arbitrary date nobody hit.
    const uint32 packed = PackClientDate(2026, 9, 3, 12, 0);
    CHECK_EQ(packed, uint32(444629760));

    const time_t converted = timeBitFieldsToSecs(packed);
    const std::tm decomposed = safe_localtime(converted);

    CHECK_EQ(decomposed.tm_year + 1900, 2026);
    CHECK_EQ(decomposed.tm_mon + 1, 9);
    CHECK_EQ(decomposed.tm_mday, 3);
    CHECK_EQ(decomposed.tm_hour, 12);
    CHECK_EQ(decomposed.tm_min, 0);
}

TEST(CalendarTime_packed_bitfield_is_never_comparable_to_a_unix_timestamp)
{
    // The year field is 5 bits, so 2031-12-31 23:59 is the furthest-out date
    // the client can express at all -- and even that stays under 600 million.
    const uint32 latestExpressible = PackClientDate(2031, 12, 31, 23, 59);
    CHECK(latestExpressible < 600000000u);
    CHECK_EQ(latestExpressible, uint32(532121083));

    // Its converted form, by contrast, sits in the modern unix-time range.
    // Comparing the two forms directly -- as the handlers did before this
    // fix -- means the packed side loses against GameTime::GetGameTime() no
    // matter what date the client actually sent, which is the entire reason
    // "event is in the past" fired unconditionally and made calendar event
    // creation impossible.
    const time_t converted = timeBitFieldsToSecs(latestExpressible);
    CHECK(converted > time_t(1700000000));
}

TEST(CalendarTime_future_date_is_not_treated_as_past)
{
    // This is the exact case the bug in abbb8a8c3 got wrong: under the raw
    // bitfield-vs-timestamp comparison, EVERY date -- including one still
    // days away -- read as "in the past", because the packed value is
    // numerically far smaller than any real unix timestamp. A correct
    // implementation must clear this for an ordinary future date; this is
    // the test that would have failed on the original bug for any input at
    // all.
    const time_t now = time(nullptr);
    const uint32 packed = PackClientDateOffsetFromNow(now, 5 * DAY);

    CHECK(!CalendarPackedTimeIsPast(packed, now, time_t(86400L)));
}

TEST(CalendarTime_date_well_in_the_past_is_treated_as_past)
{
    // The past-date check is defence-in-depth against a modified client --
    // the retail UI already refuses to submit a past date, but the server
    // keeps checking anyway (see abbb8a8c3's commit message). A date well
    // outside the grace window must still be rejected; fixing the unit
    // mismatch must not turn this into a check that never fires.
    const time_t now = time(nullptr);
    const uint32 packed = PackClientDateOffsetFromNow(now, -30 * DAY);

    CHECK(CalendarPackedTimeIsPast(packed, now, time_t(86400L)));
}

TEST(CalendarTime_date_inside_grace_window_is_not_treated_as_past)
{
    // The live callers pass an 86400s (24h) grace period, not a strict "now"
    // cutoff, so a client whose clock is a little behind -- or whose packet
    // took a while to arrive -- isn't punished for it. Two hours ago sits
    // well inside that window and must not be rejected.
    const time_t now = time(nullptr);
    const uint32 packed = PackClientDateOffsetFromNow(now, -2 * HOUR);

    CHECK(!CalendarPackedTimeIsPast(packed, now, time_t(86400L)));
}

TEST(CalendarTime_ToTimestamp_preserves_the_exact_wall_clock_the_client_sent)
{
    // Regression test for the double timezone conversion bug: CalendarPackedTimeToTimestamp()
    // used to run timeBitFieldsToSecs()'s result -- already a correct conversion, done
    // entirely by mktime() -- through a second local/UTC shift (a since-removed
    // LocalTimeToUTCTime() helper that added _timezone/timezone on top). That moved every
    // event backwards by one zone-offset hour in any non-UTC timezone, e.g. a UTC+1 click on
    // 4 November 00:00 local got stored as 3 November 23:00 and displayed to the client as
    // happening on the 3rd.
    //
    // None of the tests above would have caught that: they only ask whether a date is judged
    // "in the past" or not, and a one-hour shift is nowhere near either threshold they use (5
    // days future, 30 days past, 2 hours inside an 86400s grace window), so it never flips
    // either verdict. This test instead pins the exact calendar fields that come back out --
    // year, month, day, hour, minute -- against the exact fields that went in, so a one-hour
    // drift fails it loudly no matter where it falls relative to any threshold.
    //
    // Calling CalendarPackedTimeToTimestamp() itself, rather than timeBitFieldsToSecs()
    // directly (as CalendarTime_packed_bitfield_round_trips_to_the_calendar_date above does),
    // is what makes this catch a revert of the actual fix: this is the function the three
    // handlers call.
    //
    // The date is a fixed calendar date rather than "now" (like the future/past tests use) so
    // this test's result doesn't depend on today's date or on DST being in or out of effect at
    // the time it runs. It also doesn't depend on the host's timezone: PackClientDate() and
    // CalendarPackedTimeToTimestamp()/safe_localtime() both read/write local wall-clock fields
    // through mktime()/localtime() in whatever zone the host is in, so the round trip must
    // reproduce the same fields regardless of which zone that is -- that symmetry is what a
    // fixed UTC+1 example date could not exercise on a machine running in a different zone.
    //
    // The bug this test guards -- CalendarPackedTimeToTimestamp() adding _timezone/timezone on
    // top of mktime()'s already-correct result -- is a no-op whenever the host's UTC offset is
    // zero. Every GitHub-hosted runner this project uses defaults to UTC -- ubuntu-latest,
    // ubuntu-24.04-arm and windows-2022 alike -- and
    // nothing in the workflow sets TZ, so without forcing a zone here this test would pass in CI
    // even with the bug fully reintroduced, and only fail on a developer's non-UTC machine --
    // which is exactly how the bug reached production the first time. ScopedTimeZone forces a
    // fixed non-UTC zone for the duration of this test and restores whatever TZ was (or was not)
    // set beforehand, so the assertion below is actually exercised on every runner, and the
    // change cannot leak into other tests.
    //
    // The wall-clock time is noon, not midnight. Midnight is the one instant a DST transition
    // can delete outright: where a "spring forward" boundary lands exactly on 00:00 there is
    // no such local time, and mktime()'s result would then depend on the DST rule rather than
    // on the bug under test. Noon can never fall in a transition gap under any rule.
    //
    // Neither the zone nor the date below actually sits near such a boundary -- CET switches
    // in late March and late October, and this date is in November -- so noon is belt and
    // braces here rather than a live hazard. It is chosen so the test stays correct if the
    // date or the forced zone is ever changed by someone who has not re-derived this.
    const ScopedTimeZone forcedZone("CET-1CEST,M3.5.0,M10.5.0/3");

    const uint32 packed = PackClientDate(2026, 11, 4, 12, 0);

    const time_t converted = CalendarPackedTimeToTimestamp(packed);
    const std::tm decomposed = safe_localtime(converted);

    CHECK_EQ(decomposed.tm_year + 1900, 2026);
    CHECK_EQ(decomposed.tm_mon + 1, 11);
    CHECK_EQ(decomposed.tm_mday, 4);
    CHECK_EQ(decomposed.tm_hour, 12);
    CHECK_EQ(decomposed.tm_min, 0);
}
