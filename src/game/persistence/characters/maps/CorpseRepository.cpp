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

#include "CorpseRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

#include <sstream>

namespace CorpseRepository
{
    void Insert(CorpseRow const& row)
    {
        std::ostringstream ss;
        ss  << "INSERT INTO `corpse` (`guid`,`player`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`time`,`corpse_type`,`instance`,`phaseMask`) VALUES ("
            << row.guid << ", "
            << row.player << ", "
            << row.x << ", "
            << row.y << ", "
            << row.z << ", "
            << row.o << ", "
            << row.map << ", "
            << row.time << ", "
            << row.type << ", "
            << row.instance << ", "
            << row.phaseMask << ")";
        CharacterDatabase.Execute(ss.str().c_str());
    }

    void DeleteOfPlayer(uint32 player)
    {
        static SqlStatementID id;

        SqlStatement stmt = CharacterDatabase.CreateStatement(id, "DELETE FROM `corpse` WHERE `player` = ? AND `corpse_type` <> '0'");
        stmt.PExecute(player);
    }

    std::optional<uint32> QueryHighestGuid()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `corpse`");
        return ReadRow<uint32>(result, [](Field* fields) { return fields[0].GetUInt32(); });
    }

    static CorpseDataRow ReadCorpseData(Field* fields)
    {
        CorpseDataRow row;
        row.guid = fields[0].GetUInt32();
        row.player = fields[1].GetUInt32();
        row.positionX = fields[2].GetFloat();
        row.positionY = fields[3].GetFloat();
        row.positionZ = fields[4].GetFloat();
        row.orientation = fields[5].GetFloat();
        row.map = fields[6].GetUInt32();
        row.time = fields[7].GetUInt64();
        row.corpseType = fields[8].GetUInt32();
        row.instance = fields[9].GetUInt32();
        row.phaseMask = fields[10].GetUInt32();
        row.gender = fields[11].GetUInt8();
        row.race = fields[12].GetUInt8();
        row.classId = fields[13].GetUInt8();
        row.playerBytes = fields[14].GetUInt32();
        row.playerBytes2 = fields[15].GetUInt32();
        row.equipmentCache = fields[16].GetCppString();
        row.guildId = fields[17].GetUInt32();
        row.playerFlags = fields[18].GetUInt32();
        return row;
    }

    std::vector<CorpseDataRow> QueryCorpses()
    {
        //                                     0            1       2                  3                  4                  5                   6
        QueryResult* result = CharacterDatabase.Query("SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, "
                                                      //   7     8             9           10           11        12      13       14             15              16                17         18
                                                      "`time`, `corpse_type`, `instance`, `phaseMask`, `gender`, `race`, `class`, `playerBytes`, `playerBytes2`, `equipmentCache`, `guildId`, `playerFlags` FROM `corpse` "
                                                      "JOIN `characters` ON `player` = `characters`.`guid` "
                                                      "LEFT JOIN `guild_member` ON `player`=`guild_member`.`guid` WHERE `corpse_type` <> 0");
        return ReadRows<CorpseDataRow>(result, ReadCorpseData);
    }

    void DeleteBonesAndOldCorpses(uint32 maxAge)
    {
        CharacterDatabase.PExecute("DELETE FROM `corpse` WHERE `corpse_type` = '0' OR `time` < (UNIX_TIMESTAMP()-'%u')", maxAge);
    }
}
