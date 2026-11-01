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

/// The bytes of the spell visual kit a trainer plays on itself and on the player it teaches.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "session/packets/spells/SpellVisualPackets.h"

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
}

// 0x55A5: a zero word, the kit, the type, then the guid's mask in the order 4 7 5 3 1 2 0 6 (high
// bit first) and its non-zero bytes, each XOR 1, in the order 0 4 1 6 7 2 3 5.
TEST(SpellVisualPackets_PlaySpellVisualKitBytes)
{
    // A creature guid, raw 0xF1300BB80000002A: bytes 2a 00 00 00 b8 0b 30 f1, mask e3, then
    // 2b b9 31 f0 0a.
    const ObjectGuid trainer(HIGHGUID_UNIT, uint32(3000), uint32(42));
    // A player guid, raw 5: only byte 0, mask 02, then 04.
    const ObjectGuid player(uint64(5));
    WorldPacket packet;

    BuildPlaySpellVisualKitPacket(packet, trainer, 0xB3, 0);
    CHECK_STR(Hex(packet), "55a5:00000000b300000000000000e32bb931f00a");
    BuildPlaySpellVisualKitPacket(packet, player, 0x016A, 1);
    CHECK_STR(Hex(packet), "55a5:000000006a010000010000000204");
}
