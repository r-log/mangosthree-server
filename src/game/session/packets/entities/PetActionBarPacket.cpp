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

#include "PetActionBarPacket.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Unit.h"

void BuildPetActionBarClearedPacket(WorldPacket& packet, PetActionBarClearedFact const& /*fact*/)
{
    packet.Initialize(SMSG_PET_SPELLS, 8);
    packet << ObjectGuid();
}

void BuildPossessBarPacket(WorldPacket& packet, ObjectGuid charm, CharmInfo& charmInfo)
{
    packet.Initialize(SMSG_PET_SPELLS, 8 + 2 + 4 + 4 + 4 * MAX_UNIT_ACTION_BAR_INDEX + 1 + 1);
    packet << charm;
    packet << uint16(0);
    packet << uint32(0);
    packet << uint32(0);

    WriteActionBar(packet, charmInfo);

    packet << uint8(0);                                       // spells count
    packet << uint8(0);                                       // cooldowns count
}

void WriteActionBar(WorldPacket& packet, CharmInfo const& info)
{
    for (uint32 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        packet << uint32(info.GetActionBarEntry(i)->packedData);
    }
}
