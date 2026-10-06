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

#include "SpellModPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

void BuildSpellModifierPacket(WorldPacket& packet, SpellModChangedFact const& fact)
{
    packet.Initialize(fact.flat ? SMSG_SET_FLAT_SPELL_MODIFIER : SMSG_SET_PCT_SPELL_MODIFIER, 4 + 4 + 1 + 1 + 4);
    packet << uint32(1);                        // count of different mod->op's in packet
    packet << uint32(fact.values.size());       // count of mods per one mod->op
    packet << uint8(fact.op);
    for (std::vector<SpellModValue>::const_iterator itr = fact.values.begin(); itr != fact.values.end(); ++itr)
    {
        packet << uint8(itr->effect);
        packet << float(itr->value);
    }
}
