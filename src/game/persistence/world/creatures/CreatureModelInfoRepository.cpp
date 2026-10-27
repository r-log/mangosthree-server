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

#include "CreatureModelInfoRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CreatureModelInfoRepository
{
    CreatureModelInfoRow ReadRow(Field* fields)
    {
        CreatureModelInfoRow row;
        row.modelid = fields[0].GetUInt32();
        row.boundingRadius = fields[1].GetFloat();
        row.combatReach = fields[2].GetFloat();
        row.gender = fields[3].GetUInt8();
        row.modelidOtherGender = fields[4].GetUInt32();
        row.modelidAlternative = fields[5].GetUInt32();
        return row;
    }

    TableRead<CreatureModelInfoRow> QueryCreatureModelInfos()
    {
        //                                                0          1                  2               3         4                       5
        return ReadWorldTable<CreatureModelInfoRow>("SELECT `modelid`, `bounding_radius`, `combat_reach`, `gender`, `modelid_other_gender`, `modelid_alternative` FROM `creature_model_info`", ReadRow);
    }
}
