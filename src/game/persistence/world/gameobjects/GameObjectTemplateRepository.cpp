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

#include "GameObjectTemplateRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace GameObjectTemplateRepository
{
    namespace
    {
        std::string ReadText(Field const& field)
        {
            char const* text = field.GetString();
            return text ? text : "";
        }
    }

    GameObjectTemplateRow ReadRow(Field* fields)
    {
        GameObjectTemplateRow row;
        row.entry = fields[0].GetUInt32();
        row.type = fields[1].GetUInt32();
        row.displayId = fields[2].GetUInt32();
        row.name = ReadText(fields[3]);
        row.iconName = ReadText(fields[4]);
        row.castBarCaption = ReadText(fields[5]);
        row.unk1 = ReadText(fields[6]);
        row.faction = fields[7].GetUInt32();
        row.flags = fields[8].GetUInt32();
        row.size = fields[9].GetFloat();
        for (uint32 i = 0; i < GAMEOBJECT_TEMPLATE_QUEST_ITEM_COLUMNS; ++i)
        {
            row.questItems[i] = fields[10 + i].GetUInt32();
        }
        for (uint32 i = 0; i < GAMEOBJECT_TEMPLATE_DATA_COLUMNS; ++i)
        {
            row.data[i] = fields[16 + i].GetUInt32();
        }
        row.unk2 = fields[48].GetUInt32();
        row.minGold = fields[49].GetUInt32();
        row.maxGold = fields[50].GetUInt32();
        return row;
    }

    TableRead<GameObjectTemplateRow> QueryGameObjectTemplates()
    {
        //                                                    0        1       2            3       4           5                 6       7          8        9       10            11            12            13            14            15            16       17       18       19       20       21       22       23       24       25       26        27        28        29        30        31        32        33        34        35        36        37        38        39        40        41        42        43        44        45        46        47        48      49         50
        return ReadWorldTable<GameObjectTemplateRow>("SELECT `entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `faction`, `flags`, `size`, `questItem1`, `questItem2`, `questItem3`, `questItem4`, `questItem5`, `questItem6`, `data0`, `data1`, `data2`, `data3`, `data4`, `data5`, `data6`, `data7`, `data8`, `data9`, `data10`, `data11`, `data12`, `data13`, `data14`, `data15`, `data16`, `data17`, `data18`, `data19`, `data20`, `data21`, `data22`, `data23`, `data24`, `data25`, `data26`, `data27`, `data28`, `data29`, `data30`, `data31`, `unk2`, `mingold`, `maxgold` FROM `gameobject_template`", ReadRow);
    }
}
