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

#ifndef MANGOS_H_CREATUREEQUIPMENTREPOSITORY
#define MANGOS_H_CREATUREEQUIPMENTREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

class Field;

/// One `creature_equip_template` row: the equipment id and its three items.
struct CreatureEquipmentRow
{
    uint32 entry = 0;
    uint32 equipentry1 = 0;
    uint32 equipentry2 = 0;
    uint32 equipentry3 = 0;
};

/// The creature equipment templates on the world database.
namespace CreatureEquipmentRepository
{
    /// Reads one row: entry, then the main hand, off-hand and ranged items.
    CreatureEquipmentRow ReadRow(Field* fields);

    /// Reads every template, synchronously, in the order the database returns them, with how the read ended.
    TableRead<CreatureEquipmentRow> QueryEquipmentTemplates();
}

#endif
