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

#ifndef MANGOS_H_CURRENCYREPOSITORY
#define MANGOS_H_CURRENCYREPOSITORY

#include "Platform/Define.h"

#include <vector>

class Field;
class SqlQueryHolder;

/// One currency's saved row: the currency id, the total, week and season counts and the flags.
struct CurrencyRow
{
    uint32 id = 0;
    uint32 totalCount = 0;
    uint32 weekCount = 0;
    uint32 seasonCount = 0;
    uint8 flags = 0;
};

/// The character's saved currency rows.
namespace CurrencyRepository
{
    /// Reads one row of the login holder's currency result: id, total, week, season, flags.
    CurrencyRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<CurrencyRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Deletes every character's row of the currency `currencyId`, not only one character's, queued.
    void DeleteEveryRowOf(uint32 currencyId);

    /// Updates the owner's row of `row.id` to the row's counts and flags, queued.
    void Update(uint32 ownerGuidLow, CurrencyRow const& row);

    /// Inserts the owner's row of `row.id` with the row's counts and flags, queued.
    void Insert(uint32 ownerGuidLow, CurrencyRow const& row);

    /// Clears every character's weekly currency counts, as the currency reset does.
    void ResetEveryWeekCount();
}

#endif
