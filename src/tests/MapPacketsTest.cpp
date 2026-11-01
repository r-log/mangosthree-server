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

/// The bytes of the map's notices: the permanent instance bind, the encounter frame and the weather.

#include "TestHarness.h"
#include "InstanceData.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "session/packets/maps/MapPackets.h"

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

// SMSG_INSTANCE_SAVE_CREATED (0x0124): a uint32 0.
TEST(MapPackets_InstanceSaveCreatedBytes)
{
    WorldPacket packet;
    BuildInstanceSaveCreatedPacket(packet);
    CHECK_STR(Hex(packet), "0124:00000000");
}

// SMSG_INSTANCE_ENCOUNTER (0x1215): the type as a uint32, then by type the unit's packed guid and a
// byte, one byte, two bytes or nothing.
TEST(MapPackets_EncounterFrameBytes)
{
    // A creature guid, raw 0xF1300BB80000002A: packed, mask f1 then 2a b8 0b 30 f1.
    const ObjectGuid creature(HIGHGUID_UNIT, uint32(3000), uint32(42));
    WorldPacket packet;

    BuildEncounterFramePacket(packet, ENCOUNTER_FRAME_ENGAGE, creature, 1, 0);
    CHECK_STR(Hex(packet), "1215:02000000f12ab80b30f101");
    BuildEncounterFramePacket(packet, ENCOUNTER_FRAME_ADD_TIMER, ObjectGuid(), 3, 9);
    CHECK_STR(Hex(packet), "1215:0500000003");
    BuildEncounterFramePacket(packet, ENCOUNTER_FRAME_UPDATE_OBJECTIVE, ObjectGuid(), 1, 2);
    CHECK_STR(Hex(packet), "1215:070000000102");
    BuildEncounterFramePacket(packet, ENCOUNTER_FRAME_UNK7, ObjectGuid(), 1, 2);
    CHECK_STR(Hex(packet), "1215:09000000");
}

// SMSG_WEATHER (0x2904): the state as a uint32, the grade as a float, then 0.
TEST(MapPackets_WeatherBytes)
{
    WorldPacket packet;
    BuildWeatherPacket(packet, WEATHER_STATE_MEDIUM_RAIN, 0.5f);
    CHECK_STR(Hex(packet), "2904:040000000000003f00");
    BuildWeatherPacket(packet, WEATHER_STATE_FINE, 0.0f);
    CHECK_STR(Hex(packet), "2904:000000000000000000");
}
