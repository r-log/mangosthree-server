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

#include "CooldownPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildCooldownEventPacket(WorldPacket& packet, CooldownEventFact const& fact)
{
    packet.Initialize(SMSG_COOLDOWN_EVENT, (4 + 8));
    packet << uint32(fact.spellId);
    packet << fact.owner;
}

void BuildClearCooldownsPacket(WorldPacket& packet, CooldownsClearedFact const& fact)
{
    ObjectGuid guid = fact.owner;

    packet.Initialize(SMSG_CLEAR_COOLDOWNS, 1 + 8 + fact.spellIds.size() * 4);
    packet.WriteGuidMask<1, 3, 6>(guid);
    packet.WriteBits(fact.spellIds.size(), 24);      // cooldown count
    packet.WriteGuidMask<7, 5, 2, 4, 0>(guid);

    packet.WriteGuidBytes<7, 2, 4, 5, 1, 3>(guid);

    for (std::vector<uint32>::const_iterator itr = fact.spellIds.begin(); itr != fact.spellIds.end(); ++itr)
    {
        packet << uint32(*itr);
    }

    packet.WriteGuidBytes<0, 6>(guid);
}
