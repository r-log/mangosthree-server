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

#include "QueryPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "data/CreatureRecords.h"
#include "data/GameObjectTemplateStore.h"

void BuildGameObjectQueryResponse(WorldPacket& packet, GameObjectInfo const& info, std::string const& name,
                                  std::string const& castBarCaption)
{
    packet.Initialize(SMSG_GAMEOBJECT_QUERY_RESPONSE, 200);
    packet << uint32(info.id);
    packet << uint32(info.type);
    packet << uint32(info.displayId);
    packet << name;
    packet << uint8(0) << uint8(0) << uint8(0);
    packet << info.IconName;
    packet << castBarCaption;
    packet << info.unk1;
    for (uint32 i = 0; i < sizeof(info.raw.data) / sizeof(info.raw.data[0]); ++i)
    {
        packet << uint32(info.raw.data[i]);
    }
    packet << float(info.size);
    for (uint32 i = 0; i < sizeof(info.questItems) / sizeof(info.questItems[0]); ++i)
    {
        packet << uint32(info.questItems[i]);
    }
    packet << uint32(info.requiredLevel);
}

void BuildCreatureQueryResponse(WorldPacket& packet, CreatureInfo const& info, char const* name, char const* subName)
{
    packet.Initialize(SMSG_CREATURE_QUERY_RESPONSE, 200);
    packet << uint32(info.Entry);
    packet << (name ? name : "");
    packet << uint8(0) << uint8(0) << uint8(0);
    packet << (info.FemaleName ? info.FemaleName : "");
    packet << uint8(0) << uint8(0) << uint8(0);
    packet << (subName ? subName : "");
    packet << (info.IconName ? info.IconName : "");
    packet << uint32(info.CreatureTypeFlags);
    packet << uint32(info.CreatureTypeFlags2);
    packet << uint32(info.CreatureType);
    packet << uint32(info.Family);
    packet << uint32(info.Rank);
    packet << uint32(info.KillCredit[0]);
    packet << uint32(info.KillCredit[1]);
    for (uint32 i = 0; i < MAX_CREATURE_MODEL; ++i)
    {
        packet << uint32(info.ModelId[i]);
    }
    packet << float(info.HealthMultiplier);
    packet << float(info.PowerMultiplier);
    packet << uint8(info.RacialLeader);
    for (uint32 i = 0; i < sizeof(info.QuestItems) / sizeof(info.QuestItems[0]); ++i)
    {
        packet << uint32(info.QuestItems[i]);
    }
    packet << uint32(info.MovementTemplateId);
    packet << uint32(info.RequiredExpansion);
}
