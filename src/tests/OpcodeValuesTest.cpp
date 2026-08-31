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

TEST(OpcodeValues_calendar_replies_match_the_client)
{
    // Thirteen replies were registered against values the client's dispatcher never routes to
    // a calendar reader, so every one of these packets was silently discarded on arrival. Traced
    // each reader directly out of the client's own decompiled parser (task-4-traces.md) rather
    // than trusting a single source; every value below is corroborated by two independent
    // sources that agree.
    CHECK_EQ(int(SMSG_CALENDAR_SEND_EVENT),                    0x0C35);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_INVITE),                  0x4E16);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_INVITE_REMOVED),          0x0725);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_STATUS),                  0x2A27);
    CHECK_EQ(int(SMSG_CALENDAR_COMMAND_RESULT),                0x6F36);
    CHECK_EQ(int(SMSG_CALENDAR_RAID_LOCKOUT_ADDED),            0x2305);
    CHECK_EQ(int(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED),          0x2E25);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_INVITE_ALERT),            0x2A05);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT),    0x2617);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_REMOVED_ALERT),           0x6D35);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_UPDATED_ALERT),           0x0907);
    CHECK_EQ(int(SMSG_CALENDAR_EVENT_MODERATOR_STATUS_ALERT),  0x6B06);
    CHECK_EQ(int(SMSG_CALENDAR_CLEAR_PENDING_ACTION),          0x2106);

    // Already correct before this campaign. Pinned so a future "tidy-up" of the calendar block
    // cannot renumber the two parts that work.
    CHECK_EQ(int(SMSG_CALENDAR_SEND_CALENDAR),                 0x6805);
    CHECK_EQ(int(SMSG_CALENDAR_SEND_NUM_PENDING),              0x0C17);

    // The mass-invite pair, adopted from TrinityCore rather than proved.
    //
    // These two are the one place in this campaign where the client could not
    // settle the question. Both values reach the SAME reader (sub_1407AE660 --
    // 0x4A26 jumps into the 0x0615 case via LABEL_29), and our two builders
    // write an identical wire shape: uint32 count, then N x (packed GUID,
    // uint8). Nothing about the payload distinguishes a guild roster from an
    // arena roster, so the trace that settled the other thirteen is silent here.
    //
    // WowPacketParser annotates both as "may be swapped with" the other, which
    // is agreement that it does not know. TrinityCore ships this mapping and is
    // in production, which is corroboration rather than proof.
    //
    // Adopted deliberately, because leaving them wrong guaranteed mass invite
    // stayed broken, while a wrong guess here cannot desync or freeze anything
    // -- the shapes are identical, so the worst case is one roster populating
    // the wrong panel, which is visible in seconds and fixed by swapping these
    // two numbers.
    //
    // SETTLED ON THE WIRE, 2026-08-31. Guild mass invite was driven from a live
    // client: SMSG_CALENDAR_FILTER_GUILD went out at 0x4A26 and the client
    // populated the roster from it. That confirms the guild half by observation,
    // and the arena half follows by elimination -- the pair has only two
    // possible arrangements, so fixing one fixes the other. These are no longer
    // adopted values; they are measured ones.
    CHECK_EQ(int(SMSG_CALENDAR_FILTER_GUILD),                  0x4A26);
    CHECK_EQ(int(SMSG_CALENDAR_ARENA_TEAM),                    0x0615);

    // SMSG_CALENDAR_ARENA_TEAM and SMSG_CALENDAR_FILTER_GUILD are deliberately absent here.
    // WowPacketParser annotates the two of them as "may be swapped with" each other -- source
    // agreement exists for the pair, not for either individually, so there is no evidence to
    // prefer one assignment over the other. They are left at their pre-campaign values rather
    // than guessed at.
}
