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

#include "TalentRepository.h"
#include "Database/DatabaseEnv.h"
#include "Database/SqlOperations.h"
#include "persistence/RowReader.h"

namespace TalentRepository
{
    TalentRow ReadRow(Field* fields)
    {
        TalentRow row;
        row.talent = fields[0].GetUInt32();
        row.rank = fields[1].GetUInt32();
        row.spec = fields[2].GetUInt32();
        return row;
    }

    std::vector<TalentRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        return ReadRows<TalentRow>(holder->GetResult(slot), ReadRow);
    }

    void DeleteOwnerTalent(uint32 ownerGuidLow, uint32 talent)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u' AND `talent_id` = '%u'", ownerGuidLow, talent);
    }

    void DeleteOwnerSpec(uint32 ownerGuidLow, uint32 spec)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_talent` WHERE `guid` = '%u' AND `spec` = '%u' ", ownerGuidLow, spec);
    }

    void Save(uint32 ownerGuidLow, std::vector<TalentSaveRow> const& rows)
    {
        static SqlStatementID delTalents ;
        static SqlStatementID insTalents ;

        SqlStatement stmtDel = CharacterDatabase.CreateStatement(delTalents, "DELETE FROM `character_talent` WHERE `guid` = ? and `talent_id` = ? and `spec` = ?");
        SqlStatement stmtIns = CharacterDatabase.CreateStatement(insTalents, "INSERT INTO `character_talent` (`guid`, `talent_id`, `current_rank`, `spec`) VALUES (?, ?, ?, ?)");

        for (std::vector<TalentSaveRow>::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
        {
            if (itr->removeOld)
            {
                stmtDel.PExecute(ownerGuidLow, itr->talent, itr->spec);
            }
            if (itr->insert)
            {
                stmtIns.PExecute(ownerGuidLow, itr->talent, itr->rank, itr->spec);
            }
        }
    }
}
