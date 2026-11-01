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

#include "WorldStatePackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildInitWorldStatesPacket(WorldPacket& packet, InitWorldStatesFact const& fact)
{
    std::vector<WorldStatePair> const& states = fact.states.States();
    packet.Initialize(SMSG_INIT_WORLD_STATES, 4 + 4 + 4 + 2 + 8 * states.size());
    packet << uint32(fact.mapId);
    packet << uint32(fact.zoneId);
    packet << uint32(fact.areaId);
    packet << uint16(states.size());
    for (WorldStatePair const& pair : states)
    {
        packet << uint32(pair.state);
        packet << uint32(pair.value);
    }
}

void BuildUpdateWorldStatePacket(WorldPacket& packet, WorldStateUpdateFact const& fact)
{
    packet.Initialize(SMSG_UPDATE_WORLD_STATE, 4 + 4 + 1);
    packet << uint32(fact.state);
    packet << uint32(fact.value);
    packet << uint8(0);
}
