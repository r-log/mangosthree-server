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

#ifndef MANGOS_H_REPUTATIONREPOSITORY
#define MANGOS_H_REPUTATIONREPOSITORY

#include <vector>
#include "Platform/Define.h"

class Field;
class SqlQueryHolder;

/// One faction's saved reputation row: the faction id, the standing relative to the base and the
/// flags.
struct ReputationRow
{
    uint32 faction = 0;
    int32 standing = 0;
    uint32 flags = 0;
};

/// The character's saved reputation rows.
namespace ReputationRepository
{
    /// Reads one row of the login holder's reputation result: `faction`, `standing`, `flags`.
    ReputationRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<ReputationRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Replaces the saved row of each faction in `rows`, in order: a DELETE of the owner's row of
    /// that faction, then an INSERT of the new one, both queued on the character database.
    void Save(uint32 ownerGuidLow, std::vector<ReputationRow> const& rows);
}

#endif
