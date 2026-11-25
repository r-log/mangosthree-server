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

/// The bytes of the two aura packets a shield's hits send: the aura update that carries a shield's amount, and the
/// points-depleted notice of a shield the hit used up.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "session/packets/spells/AuraPackets.h"
#include "spells/data/SpellAuraDefines.h"

#include <string>

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

    // A player guid, raw 0x0100FF0000A5003C: its bytes (low first) are g0 3c, g1 00, g2 a5, g3 00, g4 00, g5 ff,
    // g6 00, g7 01 -- zero and non-zero bytes on both sides of the mask, and g7 = 01, which XOR 1 writes as 00.
    const uint64 kPlayerRaw = UI64LIT(0x0100FF0000A5003C);
}

// SMSG_AURA_POINTS_DEPLETED is the 15595 client's 0x7CB7.
TEST(AuraPackets_PointsDepletedIsTheClientsOpcode)
{
    CHECK_EQ(int(SMSG_AURA_POINTS_DEPLETED), 0x7CB7);
}

// The depleted notice for guid 0x0100FF0000A5003C, slot 31, effect 1:
//   mask bits g2 g4 g1 g7 g5 g0 g3 g6 = 1 0 0 1 1 1 0 0 = 9c
//   bytes g5 g0 (each ^ 1): ff->fe, 3c->3d; the effect index 01;
//   bytes g3 g7 g4 g2: g7 01->00, g2 a5->a4 (g3 and g4 are zero); the slot 1f; bytes g6 g1: both zero.
TEST(AuraPackets_PointsDepletedBytes)
{
    WorldPacket packet;
    BuildAuraPointsDepletedPacket(packet, ObjectGuid(kPlayerRaw), 31, 1);
    CHECK_STR(Hex(packet), "7cb7:9cfe3d0100a41f");
}

// A creature guid, raw 0xF1300BB80000002A (bytes 2a 00 00 00 b8 0b 30 f1), slot 5, effect 0: five guid bytes, as the
// retail packets for a player's shield carry, and eight bytes in all.
//   mask bits g2 g4 g1 g7 g5 g0 g3 g6 = 0 1 0 1 1 1 0 1 = 5d
//   bytes g5 g0: 0b->0a, 2a->2b; the effect index 00; bytes g3 g7 g4 g2: f1->f0, b8->b9; the slot 05;
//   bytes g6 g1: 30->31.
TEST(AuraPackets_PointsDepletedBytesOfACreature)
{
    WorldPacket packet;
    BuildAuraPointsDepletedPacket(packet, ObjectGuid(HIGHGUID_UNIT, uint32(3000), uint32(42)), 5, 0);
    CHECK_STR(Hex(packet), "7cb7:5d0a2b00f0b90531");
    CHECK_EQ(packet.size(), size_t(8));
}

// A shield's update after a hit, laid out as the retail sniff's Divine Aegis update after an absorbing hit (slot 26,
// flags EffectIndex0 | NotCaster | Positive | Duration | Scalable = 0x79, level 85, 15000/14990 ms, Effect 0 Value
// 12526): the target's packed guid 0105 (raw 5), slot 1a, spell 47753 = 89ba0000, flags 7900, level 55, stack 00, no
// caster, 983a0000 8e3a0000, then the amount ee300000 -- 27 bytes for a five-byte guid, the retail length.
TEST(AuraPackets_AShieldsUpdateCarriesItsAmount)
{
    AuraUpdateRecord record;
    record.slot = 26;
    record.spellId = 47753;
    record.flags = AFLAG_EFF_INDEX_0 | AFLAG_NOT_CASTER | AFLAG_POSITIVE | AFLAG_DURATION | AFLAG_EFFECT_AMOUNT_SEND;
    record.level = 85;
    record.stack = 0;
    record.maxDuration = 15000;
    record.duration = 14990;
    record.amounts[0] = 12526;

    WorldPacket packet;
    BuildAuraUpdatePacket(packet, ObjectGuid(uint64(5)), record);
    CHECK_STR(Hex(packet), "4707:01051a89ba000079005500983a00008e3a0000ee300000");
}

// Another unit's shield: the caster's packed guid (mask a5, then 3c a5 ff 01) follows the stack; the flags hold no
// duration, so none is written; both effect bits are set, so both amounts follow, in effect order.
TEST(AuraPackets_AnUpdateWritesTheCasterAndEachAmountTheFlagsName)
{
    AuraUpdateRecord record;
    record.slot = 3;
    record.spellId = 17;
    record.flags = AFLAG_EFF_INDEX_0 | AFLAG_EFF_INDEX_1 | AFLAG_POSITIVE | AFLAG_EFFECT_AMOUNT_SEND;
    record.level = 85;
    record.stack = 1;
    record.caster = ObjectGuid(kPlayerRaw);
    record.maxDuration = 30000;     // not written: no AFLAG_DURATION
    record.duration = 29000;
    record.amounts[0] = 18372;      // 0x47C4
    record.amounts[1] = -1;
    record.amounts[2] = 99;         // not written: no third effect bit

    WorldPacket packet;
    BuildAuraUpdatePacket(packet, ObjectGuid(uint64(5)), record);
    CHECK_STR(Hex(packet), "4707:0105031100000053005501a53ca5ff01c4470000ffffffff");
}

// Without AFLAG_EFFECT_AMOUNT_SEND no amount is written, whatever the effect bits.
TEST(AuraPackets_AnUpdateWithoutTheAmountFlagWritesNoAmount)
{
    AuraUpdateRecord record;
    record.slot = 7;
    record.spellId = 139;
    record.flags = AFLAG_EFF_INDEX_0 | AFLAG_NOT_CASTER | AFLAG_POSITIVE;
    record.level = 85;
    record.stack = 1;
    record.amounts[0] = 1000;

    WorldPacket packet;
    BuildAuraUpdatePacket(packet, ObjectGuid(uint64(5)), record);
    CHECK_STR(Hex(packet), "4707:0105078b00000019005501");
}
