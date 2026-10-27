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

#ifndef MANGOS_H_TALENTREPOSITORY
#define MANGOS_H_TALENTREPOSITORY

#include <vector>
#include "Platform/Define.h"

class Field;
class SqlQueryHolder;

/// One saved talent: the talent id, the rank index learned and the spec.
struct TalentRow
{
    uint32 talent = 0;
    uint32 rank = 0;
    uint32 spec = 0;
};

/// One talent the save writes: the DELETE of its old row when `removeOld`, then the INSERT of its
/// row when `insert`.
struct TalentSaveRow
{
    uint32 talent = 0;
    uint32 rank = 0;
    uint32 spec = 0;
    bool removeOld = false;
    bool insert = false;
};

/// The character's saved talents. Every write is queued on the character database.
namespace TalentRepository
{
    /// Reads one row of the login holder's talent result: talent_id, current_rank, spec.
    TalentRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<TalentRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Deletes the owner's rows of the talent `talent`.
    void DeleteOwnerTalent(uint32 ownerGuidLow, uint32 talent);

    /// Deletes the owner's rows of the spec `spec`.
    void DeleteOwnerSpec(uint32 ownerGuidLow, uint32 spec);

    /// Writes `rows` in order, each its DELETE and then its INSERT as its flags say.
    void Save(uint32 ownerGuidLow, std::vector<TalentSaveRow> const& rows);
}

#endif
