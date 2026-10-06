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

/// The session's spell modifier packet, built from the spell modifier manager's fact: the bytes of
/// the builder for one and for four pairs under both opcodes, the same bytes the count written
/// back into a placeholder gives, and the callback the session installs on a character, which
/// builds the packet at each report and sends it to the session the character holds then.
///
/// The goldens are the client's layout: uint32 1 (one operation), uint32 the number of pairs,
/// uint8 the operation, then per pair uint8 the effect bit and float the sum, little-endian. The
/// four-pair ones are Inner Focus's four packets of scenario 934, and the one-pair one is a
/// packet of scenario 926: the FNV-1a of each golden's bytes is the `fnv` the harness baseline
/// (harness-dfb8bbd80) records for it.

#include "TestHarness.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "session/packets/PlayerPacketSinks.h"
#include "session/packets/spells/SpellModPackets.h"

#include <string>
#include <vector>

namespace
{
    std::string Hex(WorldPacket const& packet)
    {
        static const char* digits = "0123456789abcdef";
        std::string text;
        uint16 opcode = packet.GetOpcode();
        for (int shift = 12; shift >= 0; shift -= 4)
        {
            text += digits[(opcode >> shift) & 0x0F];
        }
        text += ":";
        text += testing::BytesToHex(packet.contents(), packet.size());
        return text;
    }

    uint32 Fnv1a(WorldPacket const& packet)
    {
        uint32 hash = 2166136261u;
        for (size_t i = 0; i < packet.size(); ++i)
        {
            hash ^= packet.contents()[i];
            hash *= 16777619u;
        }
        return hash;
    }

    SpellModChangedFact Fact(bool flat, uint8 op, std::vector<SpellModValue> const& values)
    {
        SpellModChangedFact fact;
        fact.flat = flat;
        fact.op = op;
        fact.values = values;
        return fact;
    }

    WorldPacket Built(SpellModChangedFact const& fact)
    {
        WorldPacket packet;
        BuildSpellModifierPacket(packet, fact);
        return packet;
    }

    /// The packet as the character's own statements wrote it before the fact: the count of pairs
    /// as a placeholder, written back once the pairs are in.
    WorldPacket WrittenBack(SpellModChangedFact const& fact)
    {
        uint16 opcode = fact.flat ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER;
        uint32 modTypeCount = 0;
        WorldPacket data(opcode, 4 + 4 + 1 + 1 + 4);
        data << uint32(1);
        size_t writePos = data.wpos();
        data << uint32(modTypeCount);
        data << uint8(fact.op);
        for (SpellModValue const& value : fact.values)
        {
            data << uint8(value.effect);
            data << float(value.value);
            ++modTypeCount;
        }
        data.put<uint32>(writePos, modTypeCount);
        return data;
    }

    std::vector<SpellModValue> Focus(int32 value)
    {
        return { { 9, value }, { 11, value }, { 12, value }, { 34, value } };
    }

    /// Stands in for the session: records "<opcode>:<hex>" per packet it is sent.
    struct FakeSession
    {
        std::vector<std::string> sent;

        void SendPacket(WorldPacket const* packet)
        {
            sent.push_back(Hex(*packet));
        }
    };

    /// Stands in for the character: GetSession() answers whichever session it holds now.
    struct FakeOwner
    {
        FakeSession* session = NULL;

        FakeSession* GetSession() const
        {
            return session;
        }
    };
}

// The opcodes the fact's kind chooses between.
TEST(SpellModPackets_Opcodes)
{
    CHECK_EQ(Built(Fact(true, 7, Focus(25))).GetOpcode(), uint16(SMSG_SET_FLAT_SPELL_MODIFIER));
    CHECK_EQ(Built(Fact(false, 14, Focus(-100))).GetOpcode(), uint16(SMSG_SET_PCT_SPELL_MODIFIER));
    CHECK_EQ(uint32(SMSG_SET_FLAT_SPELL_MODIFIER), uint32(0x2834));
    CHECK_EQ(uint32(SMSG_SET_PCT_SPELL_MODIFIER), uint32(0x0224));
}

// One pair: 926's percentage packets (op 3 and op 12, bit 58, 0.0), and a flat one (op 7, bit 63,
// 25.0 = 0x41c80000).
TEST(SpellModPackets_OnePairBytes)
{
    WorldPacket first = Built(Fact(false, 3, { { 58, 0 } }));
    CHECK_STR(Hex(first), "0224:0100000001000000033a00000000");
    CHECK_EQ(Fnv1a(first), uint32(0xef1a4908));

    WorldPacket second = Built(Fact(false, 12, { { 58, 0 } }));
    CHECK_STR(Hex(second), "0224:01000000010000000c3a00000000");
    CHECK_EQ(Fnv1a(second), uint32(0x4e90ad23));

    CHECK_STR(Hex(Built(Fact(true, 7, { { 63, 25 } }))), "2834:0100000001000000073f0000c841");
}

// Four pairs: Inner Focus applied (-100.0 = 0xc2c80000 on the cost, 25.0 on the critical chance)
// and removed (0.0), on bits 9, 11, 12 and 34.
TEST(SpellModPackets_FourPairBytes)
{
    WorldPacket costApplied = Built(Fact(false, 14, Focus(-100)));
    CHECK_STR(Hex(costApplied), "0224:01000000040000000e090000c8c20b0000c8c20c0000c8c2220000c8c2");
    CHECK_EQ(Fnv1a(costApplied), uint32(0x7dabca30));

    WorldPacket critApplied = Built(Fact(true, 7, Focus(25)));
    CHECK_STR(Hex(critApplied), "2834:010000000400000007090000c8410b0000c8410c0000c841220000c841");
    CHECK_EQ(Fnv1a(critApplied), uint32(0x1f4e6517));

    WorldPacket costRemoved = Built(Fact(false, 14, Focus(0)));
    CHECK_STR(Hex(costRemoved), "0224:01000000040000000e09000000000b000000000c000000002200000000");
    CHECK_EQ(Fnv1a(costRemoved), uint32(0x16d7a174));

    WorldPacket critRemoved = Built(Fact(true, 7, Focus(0)));
    CHECK_STR(Hex(critRemoved), "2834:01000000040000000709000000000b000000000c000000002200000000");
    CHECK_EQ(Fnv1a(critRemoved), uint32(0x5d086623));
}

// The count written up front is the count the placeholder was written back with: the same bytes
// for no pair, one, four and every bit.
TEST(SpellModPackets_TheCountUpFrontIsTheCountWrittenBack)
{
    std::vector<SpellModValue> every;
    for (int bit = 0; bit < 96; ++bit)
    {
        every.push_back({ uint8(bit), int32(bit * 3 - 100) });
    }
    std::vector<SpellModChangedFact> facts = {
        Fact(true, 0, {}),
        Fact(false, 3, { { 58, 0 } }),
        Fact(false, 14, Focus(-100)),
        Fact(true, 30, every),
    };
    for (SpellModChangedFact const& fact : facts)
    {
        CHECK_STR(Hex(Built(fact)), Hex(WrittenBack(fact)));
    }
}

// The installed callback builds the fact's packet and sends it to the session the character
// holds at that report.
TEST(SpellModPackets_TheCallbackSendsToTheSessionHeldAtTheReport)
{
    FakeSession first;
    FakeSession second;
    FakeOwner owner;
    owner.session = &first;
    SpellModChangedSink sink = FactToSession(&owner, &BuildSpellModifierPacket);

    sink(Fact(false, 3, { { 58, 0 } }));
    owner.session = &second;
    sink(Fact(true, 7, { { 63, 25 } }));

    REQUIRE(first.sent.size() == 1u);
    CHECK_STR(first.sent[0], "0224:0100000001000000033a00000000");
    REQUIRE(second.sent.size() == 1u);
    CHECK_STR(second.sent[0], "2834:0100000001000000073f0000c841");
}
