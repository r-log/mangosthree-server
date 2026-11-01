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

#include "CastTargetsPacket.h"
#include "ByteBuffer.h"
#include "GameObject.h"
#include "Item.h"
#include "Unit.h"
#include "spells/pipeline/Spell.h"

void WriteCastTargets(ByteBuffer& data, SpellCastTargets const& targets)
{
    data << uint32(targets.m_targetMask);

    if (targets.m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_PVP_CORPSE | TARGET_FLAG_OBJECT | TARGET_FLAG_CORPSE | TARGET_FLAG_UNK2))
    {
        if (targets.m_targetMask & TARGET_FLAG_UNIT)
        {
            if (targets.getUnitTarget())
            {
                data << targets.getUnitTarget()->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (targets.m_targetMask & TARGET_FLAG_OBJECT)
        {
            if (targets.getGOTarget())
            {
                data << targets.getGOTarget()->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (targets.m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
        {
            data << targets.getCorpseTargetGuid().WriteAsPacked();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (targets.m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (targets.getItemTarget())
        {
            data << targets.getItemTarget()->GetPackGUID();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data << targets.getSrcTransportGuid().WriteAsPacked();
        data << targets.m_srcX << targets.m_srcY << targets.m_srcZ;
    }

    if (targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data << targets.getDestTransportGuid().WriteAsPacked();
        data << targets.m_destX << targets.m_destY << targets.m_destZ;
    }

    if (targets.m_targetMask & TARGET_FLAG_STRING)
    {
        data << targets.m_strTarget;
    }
}
