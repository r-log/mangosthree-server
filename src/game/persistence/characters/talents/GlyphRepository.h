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

#ifndef MANGOS_H_GLYPHREPOSITORY
#define MANGOS_H_GLYPHREPOSITORY

#include "Platform/Define.h"

#include <vector>

class Field;
class SqlQueryHolder;

/// One saved glyph: the spec and slot it sits in and the glyph property id.
struct GlyphRow
{
    uint8 spec = 0;
    uint8 slot = 0;
    uint32 glyph = 0;
};

/// The character's saved glyphs. Every write is queued on the character database.
namespace GlyphRepository
{
    /// Reads one row of the login holder's glyph result: spec, slot, glyph.
    GlyphRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<GlyphRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Deletes the owner's row in `slot` of `spec`, as the load does for an invalid row.
    void DeleteInvalidRow(uint32 ownerGuidLow, uint8 spec, uint8 slot);

    /// Inserts the owner's glyph in `slot` of `spec`.
    void Insert(uint32 ownerGuidLow, uint8 spec, uint8 slot, uint32 glyph);

    /// Updates the owner's glyph in `slot` of `spec`.
    void Update(uint32 ownerGuidLow, uint8 spec, uint8 slot, uint32 glyph);

    /// Deletes the owner's glyph in `slot` of `spec`, as the save does for a removed glyph.
    void Delete(uint32 ownerGuidLow, uint8 spec, uint8 slot);
}

#endif
