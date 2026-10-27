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

#include "GlyphRepository.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "persistence/RowReader.h"

namespace GlyphRepository
{
    GlyphRow ReadRow(Field* fields)
    {
        //         0     1     2
        // "SELECT spec, slot, glyph FROM character_glyphs WHERE guid='%u'"

        GlyphRow row;
        row.spec = fields[0].GetUInt8();
        row.slot = fields[1].GetUInt8();
        row.glyph = fields[2].GetUInt32();
        return row;
    }

    std::vector<GlyphRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        return ReadRows<GlyphRow>(holder->GetResult(slot), ReadRow);
    }

    void DeleteInvalidRow(uint32 ownerGuidLow, uint8 spec, uint8 slot)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_glyphs` WHERE `slot` = %u AND `spec` = %u AND `guid` = %u", slot, spec, ownerGuidLow);
    }

    void Insert(uint32 ownerGuidLow, uint8 spec, uint8 slot, uint32 glyph)
    {
        static SqlStatementID insertGlyph;

        SqlStatement stmt = CharacterDatabase.CreateStatement(insertGlyph, "REPLACE INTO `character_glyphs` (`guid`, `spec`, `slot`, `glyph`) VALUES (?, ?, ?, ?)");
        stmt.PExecute(ownerGuidLow, spec, slot, glyph);
    }

    void Update(uint32 ownerGuidLow, uint8 spec, uint8 slot, uint32 glyph)
    {
        static SqlStatementID updateGlyph;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updateGlyph, "UPDATE `character_glyphs` SET `glyph` = ? WHERE `guid` = ? AND `spec` = ? AND `slot` = ?");
        stmt.PExecute(glyph, ownerGuidLow, spec, slot);
    }

    void Delete(uint32 ownerGuidLow, uint8 spec, uint8 slot)
    {
        static SqlStatementID deleteGlyph;

        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteGlyph, "DELETE FROM `character_glyphs` WHERE `guid` = ? AND `spec` = ? AND `slot` = ?");
        stmt.PExecute(ownerGuidLow, spec, slot);
    }
}
