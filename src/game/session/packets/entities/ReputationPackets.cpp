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

#include "ReputationPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildSetForcedReactionsPacket(WorldPacket& packet, ForcedReactionsFact const& fact)
{
    packet.Initialize(SMSG_SET_FORCED_REACTIONS, 4 + fact.reactions.size() * (4 + 4));
    packet << uint32(fact.reactions.size());
    for (std::pair<uint32, uint32> const& reaction : fact.reactions)
    {
        packet << uint32(reaction.first);                   // faction_id (Faction.dbc)
        packet << uint32(reaction.second);                  // reputation rank
    }
}

void BuildSetFactionStandingPacket(WorldPacket& packet, FactionStandingFact const& fact)
{
    packet.Initialize(SMSG_SET_FACTION_STANDING, 17);
    packet << float(0);                                     // refer-a-friend bonus reputation
    packet << uint8(fact.anyRankIncreased ? 1 : 0);         // display visual effect
    packet << uint32(fact.standings.size());
    for (std::pair<uint32, uint32> const& standing : fact.standings)
    {
        packet << uint32(standing.first);
        packet << uint32(standing.second);
    }
}

void BuildInitializeFactionsPacket(WorldPacket& packet, InitialReputationsFact const& fact)
{
    packet.Initialize(SMSG_INITIALIZE_FACTIONS, (4 + 128 * 5));
    packet << uint32(0x00000100);
    for (InitialReputationsFact::Slot const& slot : fact.slots)
    {
        packet << uint8(slot.flags);
        packet << uint32(slot.standing);
    }
}

void BuildSetFactionVisiblePacket(WorldPacket& packet, FactionVisibleFact const& fact)
{
    packet.Initialize(SMSG_SET_FACTION_VISIBLE, 4);
    packet << fact.listId;
}

void BuildSetFactionAtWarPacket(WorldPacket& packet, FactionAtWarFact const& fact)
{
    packet.Initialize(SMSG_SET_FACTION_ATWAR, 4 + 1);
    packet << uint32(fact.listId);
    packet << uint8(fact.flags);
}
