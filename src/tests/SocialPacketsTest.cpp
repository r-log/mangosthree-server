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

/// The bytes of the social notices the session builds from plain values.

#include "TestHarness.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "session/packets/social/ArenaTeamPackets.h"
#include "session/packets/social/GuildPackets.h"

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

// SMSG_ARENA_TEAM_EVENT (0x0617): the event, the count of strings, each string with its 0 byte, then
// the guid when it is not empty.
TEST(SocialPackets_ArenaTeamEventBytes)
{
    // A player guid, counter 7: raw 0x0000000000000007.
    const ObjectGuid player(HIGHGUID_PLAYER, uint32(7));
    WorldPacket packet;

    // "ab" 616200 and "xy" 787900, then the guid low byte first.
    BuildArenaTeamEventPacket(packet, ERR_ARENA_TEAM_JOIN_SS, player, "ab", "xy", NULL);
    CHECK_STR(Hex(packet), "0617:0402616200787900" "0700000000000000");
    // No guid and one string.
    BuildArenaTeamEventPacket(packet, ERR_ARENA_TEAM_DISBANDED_S, ObjectGuid(), "c", NULL, NULL);
    CHECK_STR(Hex(packet), "0617:09016300");
    // Three strings.
    BuildArenaTeamEventPacket(packet, ERR_ARENA_TEAM_LEADER_CHANGED_SSS, ObjectGuid(), "a", "b", "c");
    CHECK_STR(Hex(packet), "0617:0803610062006300");
}

// SMSG_GUILD_EVENT (0x0705): the event, the count of strings, each string with its 0 byte, then the
// guid when it is not empty.
TEST(SocialPackets_GuildEventBytes)
{
    // A player guid, counter 7: raw 0x0000000000000007.
    const ObjectGuid player(HIGHGUID_PLAYER, uint32(7));
    WorldPacket packet;

    BuildGuildEventPacket(packet, GE_JOINED, player, "ab", NULL, NULL);
    CHECK_STR(Hex(packet), "0705:0401616200" "0700000000000000");
    BuildGuildEventPacket(packet, GE_DISBANDED, ObjectGuid(), NULL, NULL, NULL);
    CHECK_STR(Hex(packet), "0705:0900");
    BuildGuildEventPacket(packet, GE_PROMOTION, ObjectGuid(), "a", "b", "c");
    CHECK_STR(Hex(packet), "0705:0103610062006300");
}