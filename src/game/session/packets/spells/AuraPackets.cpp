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
    /// Writes `record` as the aura update packets carry it: its slot, its spell, its flags, its caster's level and its
    /// stack, its caster's packed guid unless the flags say the target cast it, its durations and the amount of each
    /// effect the flags name.
    void WriteAuraRecord(WorldPacket& packet, AuraUpdateRecord const& record)
    {
        packet << uint8(record.slot);
        packet << uint32(record.spellId);
        packet << uint16(record.flags);
        packet << uint8(record.level);
        packet << uint8(record.stack);

        if (!(record.flags & AFLAG_NOT_CASTER))
        {
            packet << record.caster.WriteAsPacked();
        }

        if (record.flags & AFLAG_DURATION)
        {
            packet << uint32(record.maxDuration);
            packet << uint32(record.duration);
        }

        if (record.flags & AFLAG_EFFECT_AMOUNT_SEND)
        {
            for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (record.flags & (1 << i))
                {
                    packet << int32(record.amounts[i]);
                }
            }
        }
    }
}

AuraUpdateRecord AuraUpdateRecordOf(SpellAuraHolder const& holder)
{
    AuraUpdateRecord record;
    record.slot = holder.GetAuraSlot();
    record.spellId = holder.GetId();
    record.flags = holder.GetAuraFlags();
    record.level = holder.GetAuraLevel();

    const uint32 stackCount = holder.GetAuraCharges() ? holder.GetAuraCharges() * holder.GetStackAmount()
                                                      : holder.GetStackAmount();
    record.stack = uint8(stackCount <= 255 ? stackCount : 255);

    record.caster = holder.GetCasterGuid();
    record.maxDuration = holder.GetAuraMaxDuration();
    record.duration = holder.GetAuraDuration();
    for (uint8 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (Aura const* aura = holder.GetAuraByEffectIndex(SpellEffectIndex(i)))
        {
            record.amounts[i] = aura->GetModifier()->m_amount;
        }
    }
    return record;
}

void BuildAuraUpdatePacket(WorldPacket& packet, ObjectGuid target, AuraUpdateRecord const& record)
{
    packet.Initialize(SMSG_AURA_UPDATE);
    packet << target.WriteAsPacked();
    WriteAuraRecord(packet, record);
}

void BuildAuraUpdatePacket(WorldPacket& packet, SpellAuraHolder const& holder, bool removed)
{
    if (!removed)
    {
        BuildAuraUpdatePacket(packet, holder.GetTarget()->GetObjectGuid(), AuraUpdateRecordOf(holder));
        return;
    }

    packet.Initialize(SMSG_AURA_UPDATE);
    packet << holder.GetTarget()->GetObjectGuid().WriteAsPacked();
    packet << uint8(holder.GetAuraSlot());
    packet << uint32(0);
}

void BuildAuraUpdateAllPacket(WorldPacket& packet, Unit const& target)
{
    packet.Initialize(SMSG_AURA_UPDATE_ALL);
    packet << target.GetObjectGuid().WriteAsPacked();

    Unit::VisibleAuraMap const& visibleAuras = target.GetVisibleAuras();
    for (Unit::VisibleAuraMap::const_iterator itr = visibleAuras.begin(); itr != visibleAuras.end(); ++itr)
    {
        WriteAuraRecord(packet, AuraUpdateRecordOf(*itr->second));
    }
}

void BuildAuraPointsDepletedPacket(WorldPacket& packet, ObjectGuid target, uint8 slot, uint8 effectIndex)
{
    packet.Initialize(SMSG_AURA_POINTS_DEPLETED, 1 + 8 + 2);
    packet.WriteGuidMask<2, 4, 1, 7, 5, 0, 3, 6>(target);
    packet.WriteGuidBytes<5, 0>(target);
    packet << uint8(effectIndex);
    packet.WriteGuidBytes<3, 7, 4, 2>(target);
    packet << uint8(slot);
    packet.WriteGuidBytes<6, 1>(target);
}

void SessionAuraNotices::AuraUpdate(SpellAuraHolder const& holder, bool removed)
{
    WorldPacket data;
    BuildAuraUpdatePacket(data, holder, removed);
    holder.GetTarget()->SendMessageToSet(&data, true);
}

void SessionAuraNotices::AuraPointsDepleted(SpellAuraHolder const& holder, uint8 effectIndex)
{
    WorldPacket data;
    BuildAuraPointsDepletedPacket(data, holder.GetTarget()->GetObjectGuid(), holder.GetAuraSlot(), effectIndex);
    holder.GetTarget()->SendMessageToSet(&data, true);
}

void SessionAuraNotices::VisibleAuras(Player& viewer, Unit const& target)
{
    WorldPacket data;
    BuildAuraUpdateAllPacket(data, target);
    viewer.SendDirectMessage(&data);
}
