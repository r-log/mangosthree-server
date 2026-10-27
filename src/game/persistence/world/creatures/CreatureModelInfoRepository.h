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

#ifndef MANGOS_H_CREATUREMODELINFOREPOSITORY
#define MANGOS_H_CREATUREMODELINFOREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

class Field;

/// One `creature_model_info` row: the display id, its bounding radius and combat reach, its gender
/// and the display ids of its other gender and its alternative.
struct CreatureModelInfoRow
{
    uint32 modelid = 0;
    float boundingRadius = 0.0f;
    float combatReach = 0.0f;
    uint8 gender = 0;
    uint32 modelidOtherGender = 0;
    uint32 modelidAlternative = 0;
};

/// The creature model infos on the world database.
namespace CreatureModelInfoRepository
{
    /// Reads one row: display id, bounding radius, combat reach, gender, other gender, alternative.
    CreatureModelInfoRow ReadRow(Field* fields);

    /// Reads every model info, synchronously, in the order the database returns them, with how the read ended.
    TableRead<CreatureModelInfoRow> QueryCreatureModelInfos();
}

#endif
