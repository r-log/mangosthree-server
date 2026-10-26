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

#ifndef MANGOS_H_SPELLCOOLDOWNREPOSITORY
#define MANGOS_H_SPELLCOOLDOWNREPOSITORY

#include <ctime>
#include <vector>
#include "Platform/Define.h"

class Field;
class SqlQueryHolder;

/// One saved spell cooldown: the spell, the item that started it and the time it ends.
struct SpellCooldownRow
{
    uint32 spell = 0;
    uint32 item = 0;
    time_t end = 0;
};

/// The character's saved spell cooldowns.
namespace SpellCooldownRepository
{
    /// Reads one row of the login holder's cooldown result: spell, item, time.
    SpellCooldownRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<SpellCooldownRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Deletes every saved cooldown of the owner, queued.
    void DeleteAll(uint32 ownerGuidLow);

    /// Inserts one cooldown of the owner, queued.
    void Insert(uint32 ownerGuidLow, uint32 spell, uint16 item, uint64 end);
}

#endif
