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

/// The gameobject and creature query replies hold every field the 15595 client reads, in its order,
/// and nothing after them.

#include "TestHarness.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "data/CreatureRecords.h"
#include "data/GameObjectTemplateStore.h"
#include "session/packets/entities/QueryPackets.h"

#include <string>

TEST(QueryPackets_GameObjectReplyHoldsEveryFieldTheClientReads)
{
    char icon[] = "Interact";
    char closeText[] = "close";
    GameObjectInfo info = GameObjectInfo();
    info.id = 209308;
    info.type = GAMEOBJECT_TYPE_CHEST;
    info.displayId = 10630;
    info.IconName = icon;
    info.unk1 = closeText;
    for (uint32 i = 0; i < 32; ++i)
    {
        info.raw.data[i] = 1000 + i;
    }
    info.size = 1.5f;
    for (uint32 i = 0; i < 6; ++i)
    {
        info.questItems[i] = 72000 + i;
    }
    info.requiredLevel = 85;

    WorldPacket packet;
    BuildGameObjectQueryResponse(packet, info, "a chest", "Opening");

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_GAMEOBJECT_QUERY_RESPONSE));
    uint32 entry = 0, type = 0, display = 0;
    packet >> entry >> type >> display;
    CHECK_EQ(entry, 209308u);
    CHECK_EQ(type, uint32(GAMEOBJECT_TYPE_CHEST));
    CHECK_EQ(display, 10630u);
    std::string text;
    packet >> text;
    CHECK_STR(text.c_str(), "a chest");
    for (uint32 i = 0; i < 3; ++i)
    {
        packet >> text;
        CHECK_STR(text.c_str(), "");
    }
    packet >> text;
    CHECK_STR(text.c_str(), "Interact");
    packet >> text;
    CHECK_STR(text.c_str(), "Opening");
    packet >> text;
    CHECK_STR(text.c_str(), "close");
    for (uint32 i = 0; i < 32; ++i)
    {
        uint32 word = 0;
        packet >> word;
        CHECK_EQ(word, 1000 + i);
    }
    float scale = 0.0f;
    packet >> scale;
    CHECK(scale == 1.5f);
    for (uint32 i = 0; i < 6; ++i)
    {
        uint32 item = 0;
        packet >> item;
        CHECK_EQ(item, 72000 + i);
    }
    uint32 requiredLevel = 0;
    packet >> requiredLevel;
    CHECK_EQ(requiredLevel, 85u);
    CHECK_EQ(packet.rpos(), packet.size());
}

TEST(QueryPackets_CreatureReplyHoldsEveryFieldTheClientReads)
{
    char female[] = "Scarlet Priestess";
    char icon[] = "Directions";
    CreatureInfo info = CreatureInfo();
    info.Entry = 10608;
    info.FemaleName = female;
    info.IconName = icon;
    info.CreatureTypeFlags = 0x1000;
    info.CreatureTypeFlags2 = 6;
    info.CreatureType = 7;
    info.Family = 0;
    info.Rank = 1;
    info.KillCredit[0] = 101;
    info.KillCredit[1] = 102;
    for (uint32 i = 0; i < MAX_CREATURE_MODEL; ++i)
    {
        info.ModelId[i] = 2000 + i;
    }
    info.HealthMultiplier = 2.5f;
    info.PowerMultiplier = 0.5f;
    info.RacialLeader = 1;
    for (uint32 i = 0; i < 6; ++i)
    {
        info.QuestItems[i] = 3000 + i;
    }
    info.MovementTemplateId = 7;
    info.RequiredExpansion = 3;

    WorldPacket packet;
    BuildCreatureQueryResponse(packet, info, "Scarlet Priest", "Scarlet Crusade");

    CHECK_EQ(packet.GetOpcode(), uint16(SMSG_CREATURE_QUERY_RESPONSE));
    uint32 entry = 0;
    packet >> entry;
    CHECK_EQ(entry, 10608u);
    std::string text;
    char const* names[] = { "Scarlet Priest", "", "", "", "Scarlet Priestess", "", "", "", "Scarlet Crusade", "Directions" };
    for (uint32 i = 0; i < 10; ++i)
    {
        packet >> text;
        CHECK_STR(text.c_str(), names[i]);
    }
    uint32 expected[] = { 0x1000, 6, 7, 0, 1, 101, 102, 2000, 2001, 2002, 2003 };
    for (uint32 i = 0; i < 11; ++i)
    {
        uint32 word = 0;
        packet >> word;
        CHECK_EQ(word, expected[i]);
    }
    float health = 0.0f, power = 0.0f;
    packet >> health >> power;
    CHECK(health == 2.5f);
    CHECK(power == 0.5f);
    uint8 leader = 0;
    packet >> leader;
    CHECK_EQ(leader, uint8(1));
    for (uint32 i = 0; i < 6; ++i)
    {
        uint32 item = 0;
        packet >> item;
        CHECK_EQ(item, 3000 + i);
    }
    uint32 movement = 0, expansion = 0;
    packet >> movement >> expansion;
    CHECK_EQ(movement, 7u);
    CHECK_EQ(expansion, 3u);
    CHECK_EQ(packet.rpos(), packet.size());
}
