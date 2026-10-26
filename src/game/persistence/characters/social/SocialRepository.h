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

#ifndef MANGOS_H_SOCIALREPOSITORY
#define MANGOS_H_SOCIALREPOSITORY

#include <functional>
#include <string>
#include <vector>
#include "Platform/Define.h"

class Field;
class QueryResult;
class SqlQueryHolder;

/// One listed character's saved row: its low guid, the flags and the note.
struct SocialRow
{
    uint32 friendGuid = 0;
    uint32 flags = 0;
    std::string note;
};

/// The character's saved friend and ignore list. Every write is queued on the character database;
/// the one read of another character's row is asynchronous.
namespace SocialRepository
{
    /// Reads one row of the login holder's social result: friend, flags, note.
    SocialRow ReadRow(Field* fields);

    /// Reads a holder slot's social rows with ReadRow.
    std::vector<SocialRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// ORs `flag` into the owner's row of the listed character.
    void AddFlag(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flag);

    /// Inserts the owner's row of a newly listed character with `flags`.
    void Insert(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flags);

    /// Deletes the owner's row of the listed character.
    void Delete(uint32 ownerGuidLow, uint32 friendGuidLow);

    /// Clears `flag` in the owner's row of the listed character.
    void RemoveFlag(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flag);

    /// Sets the note of the owner's row of the listed character, as a bound parameter.
    void SetNote(uint32 ownerGuidLow, uint32 friendGuidLow, std::string const& note);

    /// Asks for the flags of an owner's row of a listed character, for an owner whose list is not
    /// loaded; the callback runs on the world thread with the result, which it owns.
    void QueryFlags(uint32 ownerGuidLow, uint32 friendGuidLow, std::function<void(QueryResult*)> callback);
}

#endif
