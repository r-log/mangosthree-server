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

// Opcode values, pinned to the numbers the 4.3.4 client actually uses.
//
// These are literals on purpose. The enum is the thing under test, so deriving
// the expected value from the enum would assert nothing. Each number below was
// taken from two independent sources that agree and neither of which annotates
// doubt: the dispatch table reversed from Wow-64.exe, and WowPacketParser's
// direction-keyed 4.3.4 table. See OPCODE_CORRECTION_DESIGN_2026-08-31.md.

#include "TestHarness.h"

#include "Opcodes.h"

TEST(OpcodeValues_calendar_requests_match_the_client)
{
    // Eleven handlers were registered against values the client never sends, so
    // every calendar operation was rejected as an unknown opcode before it
    // reached any handler. The calendar LIST worked throughout, because
    // CMSG_CALENDAR_GET_CALENDAR (0x2814) was already correct -- which is why
    // this looked like a partly-working feature rather than a dead one.
    CHECK_EQ(int(CMSG_CALENDAR_ARENA_TEAM),            0x0204);
    CHECK_EQ(int(CMSG_CALENDAR_COPY_EVENT),            0x0207);
    CHECK_EQ(int(CMSG_CALENDAR_EVENT_RSVP),            0x0227);
    CHECK_EQ(int(CMSG_CALENDAR_UPDATE_EVENT),          0x2114);
    CHECK_EQ(int(CMSG_CALENDAR_EVENT_STATUS),          0x2D24);
    CHECK_EQ(int(CMSG_CALENDAR_EVENT_REMOVE_INVITE),   0x4337);
    CHECK_EQ(int(CMSG_CALENDAR_GUILD_FILTER),          0x4A16);
    CHECK_EQ(int(CMSG_CALENDAR_COMPLAIN),              0x4C36);
    CHECK_EQ(int(CMSG_CALENDAR_GET_EVENT),             0x6416);
    CHECK_EQ(int(CMSG_CALENDAR_REMOVE_EVENT),          0x6636);
    CHECK_EQ(int(CMSG_CALENDAR_EVENT_MODERATOR_STATUS), 0x6B35);

    // Already correct before this campaign. Pinned so a future "tidy-up" of the
    // calendar block cannot renumber the one part that works.
    CHECK_EQ(int(CMSG_CALENDAR_GET_CALENDAR),          0x2814);
}
