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

TEST(MovementCodec_decode_reads_the_golden_back)
{
    ByteBuffer buf;
    buf.append<uint8>(0x60);
    buf << float(1.0f);
    buf << uint32(0x11223344);
    buf << uint8(0x09) << uint8(0x0B);

    Wire::MovementStatus got;
    const Wire::DecodeResult r = Wire::Decode(buf, kTinySequence, got);
    REQUIRE(r.ok());
    CHECK_EQ(r.consumed, size_t(11));
    CHECK(got == TinyFixture());
}

TEST(MovementCodec_round_trips_the_tiny_layout)
{
    ByteBuffer buf;
    Wire::Encode(buf, kTinySequence, TinyFixture());

    Wire::MovementStatus got;
    REQUIRE(Wire::Decode(buf, kTinySequence, got).ok());
    CHECK(got == TinyFixture());
}

TEST(MovementCodec_decode_reports_a_truncated_packet_instead_of_throwing)
{
    ByteBuffer full;
    Wire::Encode(full, kTinySequence, TinyFixture());

    // Every strict prefix of a valid packet must come back as Overread, never
    // as an exception or a half-filled status the caller might trust.
    for (size_t keep = 0; keep < full.size(); ++keep)
    {
        ByteBuffer cut;
        cut.append(full.contents(), keep);
        Wire::MovementStatus got;
        const Wire::DecodeResult r = Wire::Decode(cut, kTinySequence, got);
        CHECK(r.error == Wire::DecodeError::Overread);
    }
}

TEST(MovementCodec_decode_refuses_a_null_sequence)
{
    ByteBuffer buf;
    buf << uint32(0);
    Wire::MovementStatus got;
    const Wire::DecodeResult r = Wire::Decode(buf, nullptr, got);
    CHECK(r.error == Wire::DecodeError::NoSequence);
    CHECK_EQ(r.consumed, size_t(0));
}

#include "wire/MovementSequences.h"
#include "Opcodes.h"

namespace
{
    // A mover with every optional block populated, so a round trip proves each
    // element writes what it reads: on a transport, mid-fall with a direction,
    // pitched, spline-elevated, with a counter.
    Wire::MovementStatus FullFixture()
    {
        Wire::MovementStatus s;
        s.guid = 0x0F00000000000123ull;
        s.guid2 = 0x0000000000000000ull;
        s.flags = 0x00000001 | 0x00000200 | 0x00100000;   // forward | falling | swimming-ish bits, any 30-bit value
        s.flags2 = 0x00000040;
        s.has.timestamp = true;
        s.time = 0x000ABCDEu;
        s.pos = { -8949.95f, -132.493f, 83.5312f, 0.5f };
        s.has.orientation = true;
        s.has.pitch = true;
        s.pitch = -0.25f;
        s.has.spline = false;
        s.has.splineElevation = true;
        s.splineElevation = 2.5f;
        s.counter = 7;
        s.byteParam = 3;

        s.fall.present = true;
        s.fall.hasDirection = true;
        s.fall.time = 1200;
        s.fall.vertical = -7.9f;
        s.fall.horizontal = 7.0f;
        s.fall.cosAngle = 0.6f;
        s.fall.sinAngle = 0.8f;

        s.transport.present = true;
        s.transport.guid = 0x1F40000000C0FFEEull;
        s.transport.pos = { 1.5f, -2.5f, 3.5f, 1.0f };
        s.transport.time = 555;
        s.transport.hasTime2 = true;
        s.transport.time2 = 556;
        s.transport.hasTime3 = true;
        s.transport.time3 = 557;
        s.transport.seat = 2;
        return s;
    }

    // Round-trip a fixture through one opcode's layout and hand back what came out.
    Wire::MovementStatus RoundTrip(uint16 opcode, Wire::MovementStatus const& in)
    {
        ByteBuffer buf;
        Wire::Encode(buf, Wire::SequenceFor(opcode), in);
        Wire::MovementStatus out;
        const Wire::DecodeResult r = Wire::Decode(buf, Wire::SequenceFor(opcode), out);
        // CHECK, not REQUIRE: REQUIRE's bare `return;` cannot compile in a
        // function that returns a value. A failed decode leaves `out` at its
        // defaults, which the caller's equality check then reports.
        CHECK(r.ok());
        CHECK_EQ(r.consumed, buf.size());
        return out;
    }
}

TEST(MovementSequences_unknown_opcode_has_no_layout)
{
    CHECK(Wire::SequenceFor(0xFFFF) == nullptr);
    CHECK(Wire::SequenceFor(0x0000) == nullptr);
}

TEST(MovementSequences_heartbeat_round_trips_a_full_mover)
{
    // Heartbeat carries no counter and no byteParam; everything else must survive.
    Wire::MovementStatus expect = FullFixture();
    expect.counter = 0;
    expect.byteParam = 0;
    CHECK(RoundTrip(MSG_MOVE_HEARTBEAT, FullFixture()) == expect);
}

TEST(MovementSequences_player_move_round_trips_a_full_mover)
{
    Wire::MovementStatus expect = FullFixture();
    expect.counter = 0;
    expect.byteParam = 0;
    CHECK(RoundTrip(SMSG_PLAYER_MOVE, FullFixture()) == expect);
}

TEST(MovementSequences_start_forward_and_stop_round_trip_a_full_mover)
{
    Wire::MovementStatus expect = FullFixture();
    expect.counter = 0;
    expect.byteParam = 0;
    CHECK(RoundTrip(CMSG_MOVE_START_FORWARD, FullFixture()) == expect);
    CHECK(RoundTrip(CMSG_MOVE_STOP, FullFixture()) == expect);
}

TEST(MovementSequences_a_bare_mover_round_trips_without_optional_blocks)
{
    // No transport, no fall, no pitch, no spline elevation, zero flags: every
    // "has" bit takes its absent value and no optional byte is written.
    Wire::MovementStatus bare;
    bare.guid = 0x0000000000000042ull;
    bare.pos = { 10.0f, 20.0f, 30.0f, 1.25f };
    bare.time = 99;
    CHECK(RoundTrip(MSG_MOVE_HEARTBEAT, bare) == bare);
    CHECK(RoundTrip(SMSG_PLAYER_MOVE, bare) == bare);
}

TEST(MovementCodec_carries_the_movement_counter)
{
    // The legacy reader skips MSEMovementCounter; the acks P2 builds need it.
    const Wire::Element seq[] =
    {
        Wire::Element::MovementCounter,
        Wire::Element::PositionX,
        Wire::Element::End
    };
    Wire::MovementStatus in;
    in.counter = 0xDEADBEEF;
    in.pos.x = 4.0f;

    ByteBuffer buf;
    Wire::Encode(buf, seq, in);
    CHECK_BYTES(buf.contents(), buf.size(), { 0xEF, 0xBE, 0xAD, 0xDE, 0x00, 0x00, 0x80, 0x40 });

    Wire::MovementStatus out;
    REQUIRE(Wire::Decode(buf, seq, out).ok());
    CHECK_EQ(out.counter, uint32(0xDEADBEEF));
}

namespace
{
    // Position of `e` in `seq`, or -1 when the layout does not carry it.
    int IndexOf(Wire::Sequence seq, Wire::Element e)
    {
        for (int i = 0; seq[i] != Wire::Element::End; ++i)
        {
            if (seq[i] == e)
            {
                return i;
            }
        }
        return -1;
    }

    // A gated element must be preceded by the bit announcing it -- the decoder
    // can only know what to read after it has read that bit. A layout that
    // broke this would round-trip (both sides share the table) and still be
    // undecodable on the real wire.
    void CheckAnnouncedBefore(Wire::Sequence s, Wire::Element flag, Wire::Element gated)
    {
        const int gatedAt = IndexOf(s, gated);
        if (gatedAt < 0)
        {
            return; // the layout does not carry that element
        }
        const int flagAt = IndexOf(s, flag);
        CHECK(flagAt >= 0 && flagAt < gatedAt);
    }

    void CheckLayoutAnnouncesBeforeItReads(uint16 opcode)
    {
        using E = Wire::Element;
        const Wire::Sequence s = Wire::SequenceFor(opcode);
        REQUIRE(s != nullptr);

        CheckAnnouncedBefore(s, E::HasTimestamp, E::Timestamp);
        CheckAnnouncedBefore(s, E::HasPitch, E::Pitch);
        CheckAnnouncedBefore(s, E::HasOrientation, E::PositionO);
        CheckAnnouncedBefore(s, E::HasSplineElevation, E::SplineElevation);
        CheckAnnouncedBefore(s, E::HasMovementFlags, E::Flags);
        CheckAnnouncedBefore(s, E::HasMovementFlags2, E::Flags2);

        CheckAnnouncedBefore(s, E::HasFallData, E::HasFallDirection);
        CheckAnnouncedBefore(s, E::HasFallData, E::FallTime);
        CheckAnnouncedBefore(s, E::HasFallData, E::FallVerticalSpeed);
        CheckAnnouncedBefore(s, E::HasFallDirection, E::FallHorizontalSpeed);
        CheckAnnouncedBefore(s, E::HasFallDirection, E::FallCosAngle);
        CheckAnnouncedBefore(s, E::HasFallDirection, E::FallSinAngle);

        for (int i = 0; i < 8; ++i)
        {
            CheckAnnouncedBefore(s, E::HasTransportData, E(int(E::TransportGuidBit0) + i));
            CheckAnnouncedBefore(s, E::HasTransportData, E(int(E::TransportGuidByte0) + i));
        }
        CheckAnnouncedBefore(s, E::HasTransportData, E::HasTransportTime2);
        CheckAnnouncedBefore(s, E::HasTransportData, E::HasTransportTime3);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportSeat);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportPositionX);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportPositionY);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportPositionZ);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportPositionO);
        CheckAnnouncedBefore(s, E::HasTransportData, E::TransportTime);
        CheckAnnouncedBefore(s, E::HasTransportTime2, E::TransportTime2);
        CheckAnnouncedBefore(s, E::HasTransportTime3, E::TransportTime3);
    }
}

TEST(MovementSequences_every_layout_announces_before_it_reads)
{
    CheckLayoutAnnouncesBeforeItReads(MSG_MOVE_HEARTBEAT);
    CheckLayoutAnnouncesBeforeItReads(SMSG_PLAYER_MOVE);
    CheckLayoutAnnouncesBeforeItReads(CMSG_MOVE_START_FORWARD);
    CheckLayoutAnnouncesBeforeItReads(CMSG_MOVE_STOP);
}
