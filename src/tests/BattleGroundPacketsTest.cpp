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

/// The bytes of the battleground countdown and of a played sound, as the session's builders write them.

#include "TestHarness.h"
#include "WorldPacket.h"
#include "session/packets/maps/SoundPacket.h"
#include "session/packets/pvp/BattleGroundPackets.h"

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

// SMSG_START_TIMER (0x59A5): 0, the seconds left, then the countdown's length, each a uint32.
TEST(BattleGroundPackets_StartTimerBytes)
{
    WorldPacket packet;
    BuildStartTimerPacket(packet, 90, 120);
    CHECK_STR(Hex(packet), "59a5:000000005a00000078000000");
    BuildStartTimerPacket(packet, 0, 60);
    CHECK_STR(Hex(packet), "59a5:00000000000000003c000000");
}

// SMSG_PLAY_SOUND (0x2134): the sound kit as a uint32, then the source's guid, empty.
TEST(BattleGroundPackets_PlaySoundBytes)
{
    WorldPacket packet;
    BuildPlaySoundPacket(packet, 8455);
    CHECK_STR(Hex(packet), "2134:072100000000000000000000");
}
