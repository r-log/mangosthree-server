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
// on Player, ObjectMgr and sWorld, which pull in the whole game library --
// so this test pins the one piece that IS testable in isolation: the unit
// mismatch that made the comparison meaningless. It exists to fail loudly if
// a packed bitfield and a time_t are ever compared directly again anywhere
// in this codebase.

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
