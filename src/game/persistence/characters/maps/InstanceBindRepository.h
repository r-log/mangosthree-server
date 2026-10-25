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

#ifndef MANGOS_H_INSTANCEBINDREPOSITORY
#define MANGOS_H_INSTANCEBINDREPOSITORY

#include "Platform/Define.h"

/// A character's instance binds (`character_instance`), and their copy to its group's binds
/// (`group_instance`). Every write is queued on the character database.
namespace InstanceBindRepository
{
    /// Binds a character to an instance.
    void InsertBind(uint32 guid, uint32 instanceId, bool permanent);

    /// Moves a character's bind from one instance to another and sets whether it is permanent.
    void UpdateBind(uint32 newInstanceId, bool permanent, uint32 guid, uint32 oldInstanceId);

    /// Deletes a character's bind to an instance.
    void DeleteBind(uint32 guid, uint32 instanceId);

    /// Copies every bind of a character to the group binds, as the character's own.
    void CopyBindsToGroup(uint32 guid);

    /// Deletes a character's binds that are not permanent.
    void DeleteTemporaryBinds(uint32 guid);
}

#endif
