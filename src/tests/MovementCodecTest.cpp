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

// The 4.3.4 movement-status codec in proto/wire. Everything here is pure: bytes
// in, a value out, and back. The legacy reader lives in src/game (Unit.cpp) and
// stays live until P2; these tests pin the replacement before it goes anywhere
// near the server.

#include "TestHarness.h"

#include "wire/MovementStatus.h"
#include "wire/MovementElements.h"

TEST(MovementStatus_defaults_are_a_plain_standing_mover)
{
    Wire::MovementStatus s;
    CHECK_EQ(s.guid, uint64(0));
    CHECK_EQ(s.flags, uint32(0));
    CHECK_EQ(s.flags2, uint32(0));
    CHECK_EQ(s.time, uint32(0));
    CHECK_EQ(s.counter, uint32(0));
    CHECK(s.has.orientation);
    CHECK(s.has.timestamp);
    CHECK(!s.has.pitch);
    CHECK(!s.has.spline);
    CHECK(!s.has.splineElevation);
    CHECK(!s.fall.present);
    CHECK(!s.transport.present);
    CHECK_EQ(int(s.transport.seat), -1);

    Wire::MovementStatus t;
    CHECK(s == t);
    t.pos.x = 1.0f;
    CHECK(!(s == t));

    // The vocabulary is closed by End; the count is what sequence tables size by.
    CHECK(int(Wire::Element::End) > int(Wire::Element::MovementCounter));
}

#include "wire/MovementCodec.h"
#include "Utilities/ByteBuffer.h"

namespace
{
    // A deliberately tiny layout that exercises every mechanism the real ones
    // use -- bit block, a byte-aligned float after bits (forces a flush), a
    // uint32, and the mask-then-xor GUID bytes -- with bytes small enough to
    // derive by hand. Real layouts are in Task 4; they are round-tripped, not
    // hand-derived: their bytes come from the client binary in P1.
    const Wire::Element kTinySequence[] =
    {
        Wire::Element::HasTimestamp,
        Wire::Element::GuidBit3,
        Wire::Element::GuidBit0,
        Wire::Element::HasUnknownBit,
        Wire::Element::PositionX,
        Wire::Element::Timestamp,
        Wire::Element::GuidByte0,
        Wire::Element::GuidByte3,
        Wire::Element::End
    };

    Wire::MovementStatus TinyFixture()
    {
        Wire::MovementStatus s;
        s.guid = 0x000000000A000008ull; // byte0 = 0x08, byte3 = 0x0A, the rest zero
        s.has.timestamp = true;
        s.time = 0x11223344;
        s.pos.x = 1.0f;                 // 0x3F800000
        return s;
    }
}

TEST(MovementCodec_encode_matches_the_hand_derived_bytes)
{
    // Bit block, MSB first: HasTimestamp is written inverted (present -> 0),
    // GuidBit3 = 1 (0x0A != 0), GuidBit0 = 1 (0x08 != 0), unknown = 0.
    //   0b0110_0000 = 0x60, flushed by the float that follows.
    // Then 1.0f little-endian, then the timestamp little-endian, then the two
    // GUID bytes as (byte ^ 1) because their mask bits were set.
    ByteBuffer out;
    Wire::Encode(out, kTinySequence, TinyFixture());
    CHECK_BYTES(out.contents(), out.size(),
                { 0x60,
                  0x00, 0x00, 0x80, 0x3F,
                  0x44, 0x33, 0x22, 0x11,
                  0x09,
                  0x0B });
}
