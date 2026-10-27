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

#ifndef MANGOS_H_GAMEOBJECTADDONREPOSITORY
#define MANGOS_H_GAMEOBJECTADDONREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

class Field;

/// One `gameobject_addon` row: the game object spawn guid and the rotation of its transport path.
struct GameObjectAddonRow
{
    uint32 guid = 0;
    float pathRotation0 = 0.0f;
    float pathRotation1 = 0.0f;
    float pathRotation2 = 0.0f;
    float pathRotation3 = 0.0f;
};

/// The game object addons on the world database.
namespace GameObjectAddonRepository
{
    /// Reads one row: guid, then path_rotation0 to path_rotation3.
    GameObjectAddonRow ReadRow(Field* fields);

    /// Reads every row, synchronously, in the order the database returns them, with how the read ended.
    TableRead<GameObjectAddonRow> QueryGameObjectAddons();
}

#endif
