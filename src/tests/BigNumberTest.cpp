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

#include "Auth/BigNumber.h"

/**
 * @file
 * @brief Regression tests for BigNumber::AsByteArray's zero padding.
 *
 * These exist because of a real defect: the old implementation called
 * a minimal encoder, which writes the number's big-endian bytes at offset 0,
 * and then reversed the whole buffer. When the value serialised shorter than the
 * requested length, the zero padding ended up on the wrong side and every byte of
 * the value was shifted.
 *
 * The reason it survived for years is that it needs a short serialisation to
 * show, and SRP6 values are effectively uniform: a 32-byte quantity has a
 * leading zero byte about one time in 256. The symptom was a login that failed
 * for no reason and then worked again -- indistinguishable from a flaky network.
 *
 * So these cases use values that are *deterministically* short. Revert
 * BigNumber.cpp to a minimal encoder and both go red immediately; a test built on a
 * random value would pass 255 runs out of 256 and prove nothing.
 */

TEST(BigNumber_little_endian_pads_at_the_high_end)
{
    // 1 in a 4-byte little-endian buffer is 01 00 00 00.
    // The old code produced 00 00 00 01 -- the value shifted three bytes up.
    BigNumber n;
    n.SetDword(1);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 1);
    CHECK_EQ(int(le[1]), 0);
    CHECK_EQ(int(le[2]), 0);
    CHECK_EQ(int(le[3]), 0);
}

TEST(BigNumber_big_endian_pads_at_the_low_end)
{
    // The same number big-endian is 00 00 00 01. The old code produced
    // 01 00 00 00, because it wrote at offset 0 and then did not reverse.
    BigNumber n;
    n.SetDword(1);

    const uint8* be = n.AsByteArray(4, false);

    CHECK_EQ(int(be[0]), 0);
    CHECK_EQ(int(be[1]), 0);
    CHECK_EQ(int(be[2]), 0);
    CHECK_EQ(int(be[3]), 1);
}

TEST(BigNumber_padding_preserves_a_multi_byte_value)
{
    // 0x0102 asked for in 4 bytes. Little-endian: 02 01 00 00.
    BigNumber n;
    n.SetDword(0x0102);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 0x02);
    CHECK_EQ(int(le[1]), 0x01);
    CHECK_EQ(int(le[2]), 0x00);
    CHECK_EQ(int(le[3]), 0x00);
}

TEST(BigNumber_exact_length_needs_no_padding)
{
    // No padding path at all: the value already fills the buffer. Guards against
    // a "fix" that pads unconditionally.
    BigNumber n;
    n.SetDword(0x01020304);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 0x04);
    CHECK_EQ(int(le[1]), 0x03);
    CHECK_EQ(int(le[2]), 0x02);
    CHECK_EQ(int(le[3]), 0x01);
}

TEST(BigNumber_session_key_sized_padding)
{
    // The shape that actually bit: a 40-byte session key whose top byte is zero.
    // The login proof asks for exactly 40 bytes.
    BigNumber n;
    n.SetDword(0xAABBCCDD);

    const uint8* le = n.AsByteArray(40);

    CHECK_EQ(int(le[0]), 0xDD);
    CHECK_EQ(int(le[1]), 0xCC);
    CHECK_EQ(int(le[2]), 0xBB);
    CHECK_EQ(int(le[3]), 0xAA);

    bool restIsZero = true;
    for (int i = 4; i < 40; ++i)
    {
        if (le[i] != 0)
        {
            restIsZero = false;
        }
    }
    CHECK(restIsZero);
}

// ---------------------------------------------------------------------------
// The adapter's contract beyond padding: the conventions the database columns, the
// key files and the wire depend on, now that the class is a wrapper over the tree's
// own BigInt rather than over a library.
// ---------------------------------------------------------------------------

TEST(BigNumber_hex_is_uppercase_whole_bytes_and_round_trips)
{
    BigNumber n;
    n.SetDword(1);
    CHECK_STR(n.AsHexStr(), "01");        // a whole byte, never "1"
    n.SetDword(0x0A2B);
    CHECK_STR(n.AsHexStr(), "0A2B");
    BigNumber zero;
    CHECK(zero.isZero());
    CHECK_STR(zero.AsHexStr(), "0");
    CHECK_EQ(zero.GetNumBytes(), 0);

    // lowercase digits and leading zero bytes are accepted on the way in
    BigNumber k;
    k.SetHexStr("00ff10");
    CHECK_STR(k.AsHexStr(), "FF10");
    CHECK_EQ(k.GetNumBytes(), 2);
    CHECK_EQ(k.AsDword(), uint32(0xFF10));

    // a 40-byte session key survives the round trip the database gives it
    const char* K = "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
    k.SetHexStr(K);
    CHECK_EQ(k.GetNumBytes(), 40);
    CHECK_STR(k.AsHexStr(), K);
}

TEST(BigNumber_rejects_what_is_not_hex_and_sets_zero)
{
    BigNumber n;
    n.SetDword(7);
    n.SetHexStr("-1");
    CHECK(n.isZero());
    n.SetDword(7);
    n.SetHexStr("0x10");
    CHECK(n.isZero());
    n.SetDword(7);
    n.SetHexStr("");
    CHECK(n.isZero());
}

TEST(BigNumber_setqword_sets_rather_than_adds)
{
    // The old implementation added the high word into whatever was there before
    // shifting; a value set twice was wrong the second time.
    BigNumber n;
    n.SetQword(0x0102030405060708ull);
    CHECK_STR(n.AsHexStr(), "0102030405060708");
    n.SetQword(0x0102030405060708ull);
    CHECK_STR(n.AsHexStr(), "0102030405060708");
    n.SetQword(1);
    CHECK_STR(n.AsHexStr(), "01");
}

TEST(BigNumber_arithmetic_and_modexp_agree_with_the_definition)
{
    BigNumber a, b, m;
    a.SetHexStr("F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F0F1");
    b.SetHexStr("0123456789ABCDEF0123456789ABCDEF");
    m.SetHexStr("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");   // the SRP6 modulus
    CHECK((a + b - b).AsHexStr() == std::string(a.AsHexStr()));
    CHECK(((a * b) / b).AsHexStr() == std::string(a.AsHexStr()));
    CHECK(((a * b) % b).isZero());
    // g^0 = 1, g^1 = g, and (g^x)^y == g^(x*y) mod N
    BigNumber g(7), zero, one(1), x(12345), y(6789);
    CHECK_STR(g.ModExp(zero, m).AsHexStr(), "01");
    CHECK_STR(g.ModExp(one, m).AsHexStr(), "07");
    CHECK(g.ModExp(x, m).ModExp(y, m).AsHexStr() == std::string(g.ModExp(x * y, m).AsHexStr()));
    // an even modulus and a single-limb modulus take the plain path, and agree with %
    BigNumber even(1000), small(0xFFFFFFFFu), three(3);
    CHECK(three.ModExp(BigNumber(3), even).AsHexStr() == std::string("1B"));    // 27
    CHECK(three.ModExp(BigNumber(30), small).AsHexStr() == std::string(((three.Exp(BigNumber(30))) % small).AsHexStr()));
    CHECK_STR(three.Exp(BigNumber(5)).AsHexStr(), "F3");                         // 243
}

TEST(BigNumber_random_values_have_the_requested_width)
{
    for (int bits : { 32, 152, 256 })
    {
        for (int i = 0; i < 20; ++i)
        {
            BigNumber r;
            r.SetRand(bits);
            CHECK_EQ(r.GetNumBytes(), bits / 8);
            const uint8* le = r.AsByteArray(bits / 8);
            CHECK((le[bits / 8 - 1] & 0x80) != 0);   // top bit set
            CHECK((le[0] & 1) != 0);                 // odd
        }
    }
}
