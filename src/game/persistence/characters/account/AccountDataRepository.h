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

#ifndef MANGOS_H_ACCOUNTDATAREPOSITORY
#define MANGOS_H_ACCOUNTDATAREPOSITORY

#include <string>
#include "Platform/Define.h"

class QueryResult;

/// The number of tutorial flag words an account saves.
enum
{
    TUTORIAL_ROW_FLAGS = 8
};

/// The client's saved settings (`account_data`, `character_account_data`, `character_tutorial`).
/// The two loads run when a session is attached, synchronously; every write is queued on the
/// character database.
namespace AccountDataRepository
{
    /// Selects an account's own data blocks: type, time, data.
    QueryResult* QueryGlobal(uint32 accountId);

    /// Writes an account's data block of a type over the old one, in one transaction.
    void ReplaceGlobal(uint32 accountId, uint32 type, uint64 time, std::string const& data);

    /// Writes a character's data block of a type over the old one, in one transaction.
    void ReplaceCharacter(uint32 guid, uint32 type, uint64 time, std::string const& data);

    /// Selects an account's tutorial flags, the eight words in order.
    QueryResult* QueryTutorials(uint32 accountId);

    /// Updates an account's tutorial flags.
    void UpdateTutorials(uint32 accountId, uint32 const (&flags)[TUTORIAL_ROW_FLAGS]);

    /// Inserts an account's first tutorial flags.
    void InsertTutorials(uint32 accountId, uint32 const (&flags)[TUTORIAL_ROW_FLAGS]);
}

#endif
