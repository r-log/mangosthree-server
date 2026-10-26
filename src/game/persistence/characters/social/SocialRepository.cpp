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

#include "SocialRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"
#include "Database/SqlOperations.h"

#include <utility>

namespace SocialRepository
{
    SocialRow ReadRow(Field* fields)
    {
        SocialRow row;
        row.friendGuid = fields[0].GetUInt32();
        row.flags = fields[1].GetUInt32();
        row.note = fields[2].GetCppString();
        return row;
    }

    std::vector<SocialRow> ReadHolder(SqlQueryHolder* holder, size_t slot)
    {
        QueryResult* result = holder->GetResult(slot);
        return ReadRows<SocialRow>(result, ReadRow);
    }

    void AddFlag(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flag)
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` | %u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, ownerGuidLow, friendGuidLow);
    }

    void Insert(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flags)
    {
        CharacterDatabase.PExecute("INSERT INTO `character_social` (`guid`, `friend`, `flags`) VALUES ('%u', '%u', '%u')", ownerGuidLow, friendGuidLow, flags);
    }

    void Delete(uint32 ownerGuidLow, uint32 friendGuidLow)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_social` WHERE `guid` = '%u' AND `friend` = '%u'", ownerGuidLow, friendGuidLow);
    }

    void RemoveFlag(uint32 ownerGuidLow, uint32 friendGuidLow, uint32 flag)
    {
        CharacterDatabase.PExecute("UPDATE `character_social` SET `flags` = (`flags` & ~%u) WHERE `guid` = '%u' AND `friend` = '%u'", flag, ownerGuidLow, friendGuidLow);
    }

    void SetNote(uint32 ownerGuidLow, uint32 friendGuidLow, std::string const& note)
    {
        static SqlStatementID updFriendNote;
        SqlStatement update = CharacterDatabase.CreateStatement(updFriendNote,
                              "UPDATE `character_social` SET `note` = ? WHERE `guid` = ? AND `friend` = ?");
        update.addString(note);
        update.addUInt32(ownerGuidLow);
        update.addUInt32(friendGuidLow);
        update.Execute();
    }

    void QueryFlags(uint32 ownerGuidLow, uint32 friendGuidLow, std::function<void(std::optional<uint8>)> callback)
    {
        CharacterDatabase.AsyncPQuery([callback = std::move(callback)](QueryResult* result)
                                      {
                                          callback(ReadRow<uint8>(result, [](Field* fields) { return fields[0].GetUInt8(); }));
                                      },
                                      "SELECT `flags` FROM `character_social` WHERE `guid` = %u AND `friend` = %u",
                                      ownerGuidLow, friendGuidLow);
    }
}
