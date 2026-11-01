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

#include "AuraPackets.h"
#include "Opcodes.h"
#include "Player.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "spells/auras/SpellAuras.h"

namespace
{
    /// Writes `holder`'s record: its slot, its spell, its flags, its caster's level and its stack
    /// (the charges times the stack when it has charges, at most 255), its caster's packed guid unless
    /// the flags say the target cast it, its durations and the amount of each effect the flags name.
    void WriteAuraRecord(WorldPacket& packet, SpellAuraHolder const& holder)
    {
        packet << uint8(holder.GetAuraSlot());
        packet << uint32(holder.GetId());

        uint8 auraFlags = holder.GetAuraFlags();
        packet << uint16(auraFlags);
        packet << uint8(holder.GetAuraLevel());

        uint32 stackCount = holder.GetAuraCharges() ? holder.GetAuraCharges() * holder.GetStackAmount() : holder.GetStackAmount();
        packet << uint8(stackCount <= 255 ? stackCount : 255);

        if (!(auraFlags & AFLAG_NOT_CASTER))
        {
            packet << holder.GetCasterGuid().WriteAsPacked();
        }

        if (auraFlags & AFLAG_DURATION)
        {
            packet << uint32(holder.GetAuraMaxDuration());
            packet << uint32(holder.GetAuraDuration());
        }

        if (auraFlags & AFLAG_EFFECT_AMOUNT_SEND)
        {
            for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (auraFlags & (1 << i))
                {
                    if (Aura const* aura = holder.GetAuraByEffectIndex(SpellEffectIndex(i)))
                    {
                        packet << int32(aura->GetModifier()->m_amount);
                    }
                    else
                    {
                        packet << int32(0);
                    }
                }
            }
        }
    }
}

void BuildAuraUpdatePacket(WorldPacket& packet, SpellAuraHolder const& holder, bool removed)
{
    packet.Initialize(SMSG_AURA_UPDATE);
    packet << holder.GetTarget()->GetPackGUID();

    if (removed)
    {
        packet << uint8(holder.GetAuraSlot());
        packet << uint32(0);
    }
    else
    {
        WriteAuraRecord(packet, holder);
    }
}

void BuildAuraUpdateAllPacket(WorldPacket& packet, Unit const& target)
{
    packet.Initialize(SMSG_AURA_UPDATE_ALL);
    packet << target.GetPackGUID();

    Unit::VisibleAuraMap const& visibleAuras = target.GetVisibleAuras();
    for (Unit::VisibleAuraMap::const_iterator itr = visibleAuras.begin(); itr != visibleAuras.end(); ++itr)
    {
        WriteAuraRecord(packet, *itr->second);
    }
}

void SessionAuraNotices::AuraUpdate(SpellAuraHolder const& holder, bool removed)
{
    WorldPacket data;
    BuildAuraUpdatePacket(data, holder, removed);
    holder.GetTarget()->SendMessageToSet(&data, true);
}

void SessionAuraNotices::VisibleAuras(Player& viewer, Unit const& target)
{
    WorldPacket data;
    BuildAuraUpdateAllPacket(data, target);
    viewer.SendDirectMessage(&data);
}
