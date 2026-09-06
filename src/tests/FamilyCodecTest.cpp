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

#include "WorldPacket.h"
#include "Opcodes.h"

#include "wire/GuidCodec.h"
#include "wire/MovementFamilies.h"
#include "wire/MovementSequences.h"
#include "wire/MoverCodec.h"

#include <cstring>
#include <string>

namespace
{
    // A player guid whose low byte is the only non-zero one: 0x46. Its masked
    // form sets exactly one presence bit and writes exactly one byte, 0x46 ^ 1 =
    // 0x47, so every order below produces a distinct, hand-checkable packet.
    const uint64 kGuid = 0x46;

    std::string Hex(ByteBuffer const& b)
    {
        static const char* digits = "0123456789ABCDEF";
        std::string s;
        for (size_t i = 0; i < b.size(); ++i)
        {
            s += digits[b.contents()[i] >> 4];
            s += digits[b.contents()[i] & 0xF];
        }
        return s;
    }

    WorldPacket FromHex(uint16 opcode, char const* hex)
    {
        WorldPacket p(opcode, std::strlen(hex) / 2);
        for (char const* c = hex; *c; c += 2)
        {
            const int hi = (c[0] <= '9') ? c[0] - '0' : c[0] - 'A' + 10;
            const int lo = (c[1] <= '9') ? c[1] - '0' : c[1] - 'A' + 10;
            p << uint8((hi << 4) | lo);
        }
        return p;
    }
}

TEST(GuidCodec_masked_guid_writes_presence_bits_in_one_order_and_bytes_in_another)
{
    // WriteBit fills a byte from its top bit down, so an order that lists byte
    // 0 fifth puts the one set bit at 0x08.
    const uint8 mask[8]  = { 5, 7, 3, 6, 0, 4, 1, 2 };
    const uint8 bytes[8] = { 6, 2, 3, 0, 5, 7, 1, 4 };
    WorldPacket p;
    Wire::WriteGuidMask(p, kGuid, mask);
    Wire::WriteGuidBytes(p, kGuid, bytes);
    CHECK(Hex(p) == std::string("0847"));

    p.rpos(0);
    p.ResetBitReader();
    Wire::MaskedGuid g;
    Wire::DecodeResult r = Wire::Detail::Run(p, g, [&](Wire::Detail::Reader& in, Wire::MaskedGuid& out)
    {
        Wire::ReadGuidMask(in, out, mask);
        Wire::ReadGuidBytes(in, out, bytes);
    });
    CHECK(r.ok());
    CHECK_EQ(r.consumed, size_t(2));
    CHECK_EQ(g.Value(), kGuid);
}

TEST(GuidCodec_all_zero_guid_writes_a_clear_mask_and_no_bytes)
{
    // Every byte absent: WriteGuidMask writes eight clear bits (one byte, all
    // zero) and WriteGuidBytes writes nothing at all, for any order.
    const uint8 order[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    WorldPacket p;
    Wire::WriteGuidMask(p, 0, order);
    Wire::WriteGuidBytes(p, 0, order);
    CHECK(Hex(p) == std::string("00"));

    p.rpos(0);
    p.ResetBitReader();
    Wire::MaskedGuid g;
    Wire::DecodeResult r = Wire::Detail::Run(p, g, [&](Wire::Detail::Reader& in, Wire::MaskedGuid& out)
    {
        Wire::ReadGuidMask(in, out, order);
        Wire::ReadGuidBytes(in, out, order);
    });
    CHECK(r.ok());
    CHECK_EQ(r.consumed, size_t(1));
    CHECK_EQ(g.Value(), uint64(0));

    WorldPacket packed;
    Wire::WritePackedGuid(packed, 0);
    CHECK(Hex(packed) == std::string("00"));
    packed.rpos(0);
    uint64 v = 0;
    Wire::DecodeResult r2 = Wire::Detail::Run(packed, v, [](Wire::Detail::Reader& in, uint64& out) { out = Wire::ReadPackedGuid(in); });
    CHECK(r2.ok());
    CHECK_EQ(v, uint64(0));
}

TEST(GuidCodec_packed_guid_is_a_mask_byte_then_the_bytes_low_to_high)
{
    WorldPacket p;
    Wire::WritePackedGuid(p, 0x0000000000010046ULL);
    // Byte 0 is 0x46, byte 1 is zero (skipped), byte 2 is 0x01: the mask (bit 0
    // and bit 2) is 0x05, then the present bytes low to high.
    CHECK(Hex(p) == std::string("054601"));
    p.rpos(0);
    uint64 v = 0;
    Wire::DecodeResult r = Wire::Detail::Run(p, v, [](Wire::Detail::Reader& in, uint64& out) { out = Wire::ReadPackedGuid(in); });
    CHECK(r.ok());
    CHECK_EQ(v, 0x0000000000010046ULL);
    // A mask that promises a byte the buffer lacks is an overread, not a log line.
    WorldPacket shortOne = FromHex(SMSG_CLIENT_CONTROL_UPDATE, "03");
    shortOne.rpos(0);
    r = Wire::Detail::Run(shortOne, v, [](Wire::Detail::Reader& in, uint64& out) { out = Wire::ReadPackedGuid(in); });
    CHECK(r.error == Wire::DecodeError::Overread);
}

TEST(MoverCodec_active_mover_has_one_order_per_opcode)
{
    // SMSG (CPP's MoveSetActiveMover::Write): mask 5,7,3,6,0,4,1,2 / bytes 6,2,3,0,5,7,1,4.
    // CMSG (the tree's HandleSetActiveMoverOpcode, CPP's SetActiveMover::Read):
    // mask 7,2,1,0,4,5,6,3 / bytes 3,2,4,0,5,1,6,7.
    Wire::ActiveMover v;
    v.guid = kGuid;
    WorldPacket s(SMSG_MOVE_SET_ACTIVE_MOVER, 16);
    Wire::EncodeActiveMover(s, SMSG_MOVE_SET_ACTIVE_MOVER, v);
    CHECK(Hex(s) == std::string("0847"));
    WorldPacket c(CMSG_SET_ACTIVE_MOVER, 16);
    Wire::EncodeActiveMover(c, CMSG_SET_ACTIVE_MOVER, v);
    CHECK(Hex(c) == std::string("1047"));

    Wire::ActiveMover back;
    s.rpos(0); s.ResetBitReader();
    CHECK(Wire::DecodeActiveMover(s, SMSG_MOVE_SET_ACTIVE_MOVER, back).ok());
    CHECK_EQ(back.guid, kGuid);
    c.rpos(0); c.ResetBitReader();
    CHECK(Wire::DecodeActiveMover(c, CMSG_SET_ACTIVE_MOVER, back).ok());
    CHECK_EQ(back.guid, kGuid);
    // The wrong order does not read the guid back.
    c.rpos(0); c.ResetBitReader();
    CHECK(Wire::DecodeActiveMover(c, SMSG_MOVE_SET_ACTIVE_MOVER, back).ok());
    CHECK(back.guid != kGuid);
}

TEST(MoverCodec_control_update_is_a_packed_guid_and_a_byte)
{
    Wire::ControlUpdate v;
    v.guid = kGuid;
    v.allowMove = 1;
    WorldPacket p(SMSG_CLIENT_CONTROL_UPDATE, 16);
    Wire::EncodeControlUpdate(p, v);
    CHECK(Hex(p) == std::string("014601"));
    Wire::ControlUpdate back;
    p.rpos(0);
    Wire::DecodeResult r = Wire::DecodeControlUpdate(p, back);
    CHECK(r.ok());
    CHECK_EQ(r.consumed, size_t(3));
    CHECK_EQ(back.guid, kGuid);
    CHECK_EQ(int(back.allowMove), 1);
}

TEST(MovementFamilies_every_family_opcode_is_known_and_no_registry_opcode_is_a_family)
{
    CHECK(Wire::FamilyFor(SMSG_MOVE_KNOCK_BACK) == Wire::Family::KnockBack);
    CHECK(Wire::FamilyFor(SMSG_MOVE_TELEPORT) == Wire::Family::Teleport);
    CHECK(Wire::FamilyFor(CMSG_MOVE_TELEPORT_ACK) == Wire::Family::TeleportAck);
    CHECK(Wire::FamilyFor(SMSG_MOVE_SET_ACTIVE_MOVER) == Wire::Family::ActiveMover);
    CHECK(Wire::FamilyFor(CMSG_SET_ACTIVE_MOVER) == Wire::Family::ActiveMover);
    CHECK(Wire::FamilyFor(SMSG_CLIENT_CONTROL_UPDATE) == Wire::Family::ControlUpdate);
    CHECK(Wire::FamilyFor(SMSG_MONSTER_MOVE) == Wire::Family::MonsterMove);
    CHECK(Wire::FamilyFor(SMSG_MONSTER_MOVE_TRANSPORT) == Wire::Family::MonsterMove);
    CHECK(Wire::FamilyFor(CMSG_MOVE_START_FORWARD) == Wire::Family::None);
    CHECK_EQ(Wire::FamilyCount(), size_t(8));
    CHECK(Wire::IsKnown(SMSG_MOVE_KNOCK_BACK));
    CHECK(Wire::IsKnown(CMSG_MOVE_START_FORWARD));
    CHECK(!Wire::IsKnown(CMSG_PING));
    CHECK(!Wire::IsKnown(CMSG_USE_ITEM));               // embedded: not a packet layout, not a family
    for (size_t i = 0; i < Wire::FamilyCount(); ++i)
    {
        const uint16 op = Wire::FamilyOpcodeAt(i);
        CHECK_EQ(Wire::FamilyIndex(op), int(i));
        CHECK(!Wire::IsPacketLayout(op));               // the two kinds never overlap
        CHECK(Wire::SequenceFor(op) == nullptr);
    }
    CHECK(std::string(Wire::FamilyName(Wire::Family::MonsterMove)) == "monster move");
}

TEST(MovementFamilies_judge_covers_both_kinds_the_same_way)
{
    // A registry packet: judged by DecodeWhole + Encode; a family packet: by its
    // own codec. Both answer decoded/exact/consumed the same way.
    WorldPacket status = FromHex(CMSG_MOVE_START_FORWARD, "CDAC59C6E13AC9423D9A77C52011800000012F5C5A054067920100");
    Wire::Verdict v = Wire::Judge(CMSG_MOVE_START_FORWARD, status, false);
    CHECK(v.decoded);
    CHECK(v.exact);
    CHECK_EQ(v.result.consumed, size_t(27));

    WorldPacket mover = FromHex(SMSG_MOVE_SET_ACTIVE_MOVER, "0847");
    v = Wire::Judge(SMSG_MOVE_SET_ACTIVE_MOVER, mover, false);
    CHECK(v.decoded);
    CHECK(v.exact);
    CHECK_EQ(v.result.consumed, size_t(2));

    WorldPacket left = FromHex(SMSG_MOVE_SET_ACTIVE_MOVER, "0847EE");
    v = Wire::Judge(SMSG_MOVE_SET_ACTIVE_MOVER, left, false);
    CHECK(!v.decoded);
    CHECK(v.result.error == Wire::DecodeError::LeftBytes);

    WorldPacket unknown = FromHex(CMSG_PING, "0000");
    v = Wire::Judge(CMSG_PING, unknown, false);
    CHECK(!v.decoded);
    CHECK(v.result.error == Wire::DecodeError::NoSequence);
}
