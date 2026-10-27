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

#include "GameObjectAddonRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace GameObjectAddonRepository
{
    GameObjectAddonRow ReadRow(Field* fields)
    {
        GameObjectAddonRow row;
        row.guid = fields[0].GetUInt32();
        row.pathRotation0 = fields[1].GetFloat();
        row.pathRotation1 = fields[2].GetFloat();
        row.pathRotation2 = fields[3].GetFloat();
        row.pathRotation3 = fields[4].GetFloat();
        return row;
    }

    TableRead<GameObjectAddonRow> QueryGameObjectAddons()
    {
        //                                              0       1                 2                 3                 4
        return ReadWorldTable<GameObjectAddonRow>("SELECT `guid`, `path_rotation0`, `path_rotation1`, `path_rotation2`, `path_rotation3` FROM `gameobject_addon`", ReadRow);
    }
}
