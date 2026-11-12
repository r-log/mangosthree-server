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

#include "QuestCompletionNpcsPacket.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "data/QuestRelationStore.h"

#include <iterator>

void BuildQuestCompletionNpcResponse(WorldPacket& packet, QuestRelationStore const& relations, std::vector<uint32> const& questIds)
{
    packet.Initialize(SMSG_QUEST_COMPLETION_NPC_RESPONSE, 3 + questIds.size() * (3 + 4 + 4));

    packet.WriteBits(uint32(questIds.size()), 23);
    for (uint32 questId : questIds)
    {
        QuestRelationsMapBounds enders = relations.QuestEnders(questId);
        packet.WriteBits(uint32(std::distance(enders.first, enders.second)), 24);
    }
    packet.FlushBits();

    for (uint32 questId : questIds)
    {
        packet << uint32(questId);

        QuestRelationsMapBounds enders = relations.QuestEnders(questId);
        for (QuestRelationsMap::const_iterator itr = enders.first; itr != enders.second; ++itr)
        {
            packet << uint32(itr->second);
        }
    }
}
