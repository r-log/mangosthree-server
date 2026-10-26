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

#include "SpellCooldownRepository.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "persistence/RowReader.h"

namespace SpellCooldownRepository
{
    SpellCooldownRow ReadRow(Field* fields)
    {
        SpellCooldownRow row;
        row.spell = fields[0].GetUInt32();
        row.item  = fields[1].GetUInt32();
        row.end   = (time_t)fields[2].GetUInt64();
        return row;
    }

    std::vector<SpellCooldownRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        return ReadRows<SpellCooldownRow>(holder->GetResult(slot), ReadRow);
    }

    void DeleteAll(uint32 ownerGuidLow)
    {
        static SqlStatementID deleteSpellCooldown ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown, "DELETE FROM `character_spell_cooldown` WHERE `guid` = ?");
        stmt.PExecute(ownerGuidLow);
    }

    void Insert(uint32 ownerGuidLow, uint32 spell, uint16 item, uint64 end)
    {
        static SqlStatementID insertSpellCooldown ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(insertSpellCooldown, "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( ?, ?, ?, ?)");
        stmt.PExecute(ownerGuidLow, spell, item, end);
    }
}
