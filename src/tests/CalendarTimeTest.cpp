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
// comparison out of the three handlers and into CalendarPackedTimeToUtc() /
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
