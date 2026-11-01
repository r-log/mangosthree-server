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

/// The bytes of the spell effects' notices and of the cast's pushback.

#include "TestHarness.h"
#include "Object.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "session/packets/spells/SpellCastPackets.h"
#include "session/packets/spells/SpellEffectPackets.h"

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

    // A creature guid, raw 0xF1300BB80000002A: packed, mask f1 then 2a b8 0b 30 f1.
    const ObjectGuid kCreature(HIGHGUID_UNIT, uint32(3000), uint32(42));
    // A player guid, raw 5: packed, mask 01 then 05.
    const ObjectGuid kPlayer(uint64(5));
}

// SMSG_SPELL_DELAYED (0x0715): the caster's packed guid, then the pushback.
TEST(SpellEffectPackets_SpellDelayedBytes)
{
    WorldPacket packet;
    BuildSpellDelayedPacket(packet, kPlayer, 500);
    CHECK_STR(Hex(packet), "0715:0105f4010000");
}

// SMSG_SPELLDISPELLOG (0x4516) and SMSG_SPELLSTEALLOG (0x4E26): the victim's and the caster's packed
// guids, the spell, a zero byte, the count, then each spell taken with a zero byte.
TEST(SpellEffectPackets_DispelAndStealLogBytes)
{
    std::vector<uint32> dispelled;
    dispelled.push_back(589);
    dispelled.push_back(8921);
    WorldPacket packet;

    BuildDispelLogPacket(packet, false, kCreature, kPlayer, 527, dispelled);
    CHECK_STR(Hex(packet), "4516:f12ab80b30f101050f02000000020000004d02000000d922000000");

    std::vector<uint32> stolen(1, 1459);
    BuildDispelLogPacket(packet, true, kCreature, kPlayer, 30449, stolen);
    CHECK_STR(Hex(packet), "4e26:f12ab80b30f10105f17600000001000000b305000000");
}

// SMSG_DISPEL_FAILED (0x0307): the caster's and the victim's raw guids, the spell, then each spell
// not dispelled.
TEST(SpellEffectPackets_DispelFailedBytes)
{
    std::vector<uint32> failed(1, 589);
    WorldPacket packet;
    BuildDispelFailedPacket(packet, kPlayer, kCreature, 527, failed);
    CHECK_STR(Hex(packet), "0307:05000000000000002a000000b80b30f10f0200004d020000");
}

// SMSG_SPELLINSTAKILLLOG (0x6216), SMSG_CLEAR_TARGET (0x4B26) and SMSG_DUEL_REQUESTED (0x4504): raw
// guids, and the kill's spell.
TEST(SpellEffectPackets_InstaKillClearTargetAndDuelBytes)
{
    WorldPacket packet;
    BuildInstaKillLogPacket(packet, kPlayer, kCreature, 5);
    CHECK_STR(Hex(packet), "6216:05000000000000002a000000b80b30f105000000");
    BuildInstaKillLogPacket(packet, ObjectGuid(), kCreature, 5);
    CHECK_STR(Hex(packet), "6216:00000000000000002a000000b80b30f105000000");
    BuildClearTargetPacket(packet, kCreature);
    CHECK_STR(Hex(packet), "4b26:2a000000b80b30f1");
    BuildDuelRequestedPacket(packet, kCreature, kPlayer);
    CHECK_STR(Hex(packet), "4504:2a000000b80b30f10500000000000000");
}

// SMSG_TOTEM_CREATED (0x2414): the slot, the totem's raw guid, the duration, then the spell.
TEST(SpellEffectPackets_TotemCreatedBytes)
{
    WorldPacket packet;
    BuildTotemCreatedPacket(packet, 1, kCreature, 120000, 3599);
    CHECK_STR(Hex(packet), "2414:012a000000b80b30f1c0d401000f0e0000");
}

// SMSG_NEW_TAXI_PATH (0x4B35), no payload, and SMSG_TAXINODE_STATUS (0x2936): the flight master's
// raw guid, then the status.
TEST(SpellEffectPackets_TaxiNodeLearnedBytes)
{
    WorldPacket packet;
    BuildNewTaxiPathPacket(packet);
    CHECK_STR(Hex(packet), "4b35:");
    BuildTaxiNodeStatusPacket(packet, kCreature, 1);
    CHECK_STR(Hex(packet), "2936:2a000000b80b30f101");
}

// SMSG_BINDPOINTUPDATE (0x0527): the coordinates as floats, the map, then the area; SMSG_PLAYERBOUND
// (0x2516): the binder's raw guid, then the area.
TEST(SpellEffectPackets_HomeBoundBytes)
{
    WorldPacket packet;
    BuildBindPointUpdatePacket(packet, WorldLocation(1, 1.0f, -2.5f, 0.5f, 3.0f), 14);
    CHECK_STR(Hex(packet), "0527:0000803f000020c00000003f010000000e000000");
    BuildPlayerBoundPacket(packet, kCreature, 14);
    CHECK_STR(Hex(packet), "2516:2a000000b80b30f10e000000");
}