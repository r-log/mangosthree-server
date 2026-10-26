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

#ifndef MANGOS_H_DISABLEDATAREPOSITORY
#define MANGOS_H_DISABLEDATAREPOSITORY

#include "Platform/Define.h"

#include <vector>

/// One `disables` row.
struct DisableRow
{
    uint32 sourceType = 0;
    uint32 entry = 0;
    uint8 flags = 0;
    uint32 data = 0;
};

/// The disabled spells, quests, maps and other entries on the world database (`disables`): the load
/// at startup and on a reload runs synchronously and is read into rows.
namespace DisableDataRepository
{
    /// Selects every disabled entry (`disables`).
    std::vector<DisableRow> QueryDisables();
}

#endif
