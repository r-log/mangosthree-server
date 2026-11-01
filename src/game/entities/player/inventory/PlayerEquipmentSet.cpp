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

/**
 * @file PlayerEquipmentSet.cpp
 * @brief Player's equipment sets: list, save and delete.
 */

#include "Player.h"
#include "Log.h"
#include "entities/player/inventory/InventoryNotices.h"
#include "ObjectMgr.h"

void Player::SendEquipmentSetList()
{
    sInventoryNotices.EquipmentSetsShown(*this, m_EquipmentSets);
}

void Player::SetEquipmentSet(uint32 index, EquipmentSet eqset)
{
    if (eqset.Guid != 0)
    {
        bool found = false;

        for (EquipmentSets::iterator itr = m_EquipmentSets.begin(); itr != m_EquipmentSets.end(); ++itr)
        {
            if ((itr->second.Guid == eqset.Guid) && (itr->first == index))
            {
                found = true;
                break;
            }
        }

        if (!found)                                         // something wrong...
        {
            sLog.outError("Player %s tried to save equipment set " UI64FMTD " (index %u), but that equipment set not found!", GetName(), eqset.Guid, index);
            return;
        }
    }

    EquipmentSet& eqslot = m_EquipmentSets[index];

    EquipmentSetUpdateState old_state = eqslot.state;

    eqslot = eqset;

    if (eqset.Guid == 0)
    {
        eqslot.Guid = sObjectMgr.GenerateEquipmentSetGuid();

        sInventoryNotices.EquipmentSetSaved(*this, index, eqslot.Guid);
    }

    eqslot.state = old_state == EQUIPMENT_SET_NEW ? EQUIPMENT_SET_NEW : EQUIPMENT_SET_CHANGED;
}

void Player::DeleteEquipmentSet(uint64 setGuid)
{
    for (EquipmentSets::iterator itr = m_EquipmentSets.begin(); itr != m_EquipmentSets.end(); ++itr)
    {
        if (itr->second.Guid == setGuid)
        {
            if (itr->second.state == EQUIPMENT_SET_NEW)
            {
                m_EquipmentSets.erase(itr);
            }
            else
            {
                itr->second.state = EQUIPMENT_SET_DELETED;
            }
            break;
        }
    }
}
