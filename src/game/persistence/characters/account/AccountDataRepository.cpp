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

#include "AccountDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

namespace AccountDataRepository
{
    static AccountDataRow ReadAccountData(Field* fields)
    {
        AccountDataRow row;
        row.type = fields[0].GetUInt32();
        row.time = fields[1].GetUInt64();
        row.data = fields[2].GetCppString();
        return row;
    }

    std::vector<AccountDataRow> QueryGlobal(uint32 accountId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `type`, `time`, `data` FROM `account_data` WHERE `account` = '%u'", accountId);
        return ReadRows<AccountDataRow>(result, ReadAccountData);
    }

    std::vector<AccountDataRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        QueryResult* result = holder->GetResult(slot);
        return ReadRows<AccountDataRow>(result, ReadAccountData);
    }

    void ReplaceGlobal(uint32 accountId, uint32 type, uint64 time, std::string const& data)
    {
        static SqlStatementID delId;
        static SqlStatementID insId;

        CharacterDatabase.BeginTransaction();

        SqlStatement stmt = CharacterDatabase.CreateStatement(delId, "DELETE FROM `account_data` WHERE `account` = ? AND `type` = ?");
        stmt.PExecute(accountId, type);

        stmt = CharacterDatabase.CreateStatement(insId, "INSERT INTO `account_data` VALUES (?,?,?,?)");
        stmt.PExecute(accountId, type, time, data.c_str());

        CharacterDatabase.CommitTransaction();
    }

    void ReplaceCharacter(uint32 guid, uint32 type, uint64 time, std::string const& data)
    {
        static SqlStatementID delId;
        static SqlStatementID insId;

        CharacterDatabase.BeginTransaction();

        SqlStatement stmt = CharacterDatabase.CreateStatement(delId, "DELETE FROM `character_account_data` WHERE `guid` = ? AND `type` = ?");
        stmt.PExecute(guid, type);

        stmt = CharacterDatabase.CreateStatement(insId, "INSERT INTO `character_account_data` VALUES (?,?,?,?)");
        stmt.PExecute(guid, type, time, data.c_str());

        CharacterDatabase.CommitTransaction();
    }

    static TutorialRow ReadTutorials(Field* fields)
    {
        TutorialRow row;
        for (int i = 0; i < TUTORIAL_ROW_FLAGS; ++i)
        {
            row.flags[i] = fields[i].GetUInt32();
        }
        return row;
    }

    std::optional<TutorialRow> QueryTutorials(uint32 accountId)
    {
        QueryResult* result = CharacterDatabase.PQuery("SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '%u'", accountId);
        return ReadRow<TutorialRow>(result, ReadTutorials);
    }

    void UpdateTutorials(uint32 accountId, uint32 const (&flags)[TUTORIAL_ROW_FLAGS])
    {
        static SqlStatementID updTutorial;
        SqlStatement stmt = CharacterDatabase.CreateStatement(updTutorial, "UPDATE `character_tutorial` SET `tut0`=?, `tut1`=?, `tut2`=?, `tut3`=?, `tut4`=?, `tut5`=?, `tut6`=?, `tut7`=? WHERE `account` = ?");
        for (int i = 0; i < TUTORIAL_ROW_FLAGS; ++i)
        {
            stmt.addUInt32(flags[i]);
        }

        stmt.addUInt32(accountId);
        stmt.Execute();
    }

    void InsertTutorials(uint32 accountId, uint32 const (&flags)[TUTORIAL_ROW_FLAGS])
    {
        static SqlStatementID insTutorial;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insTutorial, "INSERT INTO `character_tutorial` (`account`,`tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(accountId);
        for (int i = 0; i < TUTORIAL_ROW_FLAGS; ++i)
        {
            stmt.addUInt32(flags[i]);
        }

        stmt.Execute();
    }

    void DeleteTutorials(uint32 accountId)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_tutorial` WHERE `account` = '%u'", accountId);
    }
}
