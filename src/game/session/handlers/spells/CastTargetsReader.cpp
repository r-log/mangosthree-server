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

#include "CastTargetsReader.h"
#include "ByteBuffer.h"
#include "data/MapGeometry.h"
#include "spells/pipeline/Spell.h"
#include "WorldPacket.h"
#include "movement/MovementInfo.h"

void ReadAdditionalCastData(WorldPacket& packet, uint8 castFlags, SpellCastTargets& targets)
{
    if (castFlags & 0x02)               // has trajectory data
    {
        float elevation;
        float speed;
        packet >> elevation >> speed;
        targets.SetTrajectory(elevation, speed);

        bool hasMovementData;
        packet >> hasMovementData;
        if (hasMovementData)
        {
            MovementInfo mi;
            packet >> mi;
            targets.setSource(mi.GetPos()->x, mi.GetPos()->y, mi.GetPos()->z);
        }
    }
    else if (castFlags & 0x08)          // has archaeology weight
    {
        uint32 count;
        uint8 type;
        packet >> count;
        for (uint32 i = 0; i < count; ++i)
        {
            packet >> type;
            switch (type)
            {
                case 1:                         // Fragments
                    packet >> Unused<uint32>(); // Currency entry
                    packet >> Unused<uint32>(); // Currency count
                    break;
                case 2:                         // Keystones
                    packet >> Unused<uint32>(); // Item entry
                    packet >> Unused<uint32>(); // Item count
                    break;
            }
        }
    }
}

void ReadCastTargets(ByteBuffer& packet, SpellCastTargets& targets, Unit* caster)
{
    packet >> targets.m_targetMask;

    if (targets.m_targetMask == TARGET_FLAG_SELF)
    {
        targets.SetSelfTarget(caster);
        return;
    }

    // TARGET_FLAG_UNK2 is used for non-combat pets, maybe other?
    if (targets.m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2))
    {
        ObjectGuid unit;
        packet >> unit.ReadAsPacked();
        targets.SetUnitTargetGuid(unit);
    }

    if (targets.m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        ObjectGuid object;
        packet >> object.ReadAsPacked();
        targets.SetGOTargetGuid(object);
    }

    if ((targets.m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)) && caster->GetTypeId() == TYPEID_PLAYER)
    {
        ObjectGuid item;
        packet >> item.ReadAsPacked();
        targets.SetItemTargetGuid(item);
    }

    if (targets.m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
    {
        ObjectGuid corpse;
        packet >> corpse.ReadAsPacked();
        targets.SetCorpseTargetGuid(corpse);
    }

    if (targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        ObjectGuid transport;
        float x, y, z;
        packet >> transport.ReadAsPacked();
        packet >> x >> y >> z;
        targets.SetSourceAsSent(transport, x, y, z);
        if (!MaNGOS::IsValidMapCoord(x, y, z))
        {
            throw ByteBufferException(false, packet.rpos(), 0, packet.size());
        }
    }

    if (targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        ObjectGuid transport;
        float x, y, z;
        packet >> transport.ReadAsPacked();
        packet >> x >> y >> z;
        targets.SetDestinationAsSent(transport, x, y, z);
        if (!MaNGOS::IsValidMapCoord(x, y, z))
        {
            throw ByteBufferException(false, packet.rpos(), 0, packet.size());
        }
    }

    if (targets.m_targetMask & TARGET_FLAG_STRING)
    {
        packet >> targets.m_strTarget;
    }

    targets.Update(caster);
}
