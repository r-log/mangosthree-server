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

#include "ItemDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace ItemDataRepository
{
    static ItemLocaleRow ReadItemLocale(Field* fields)
    {
        ItemLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < ITEM_LOCALE_ROW_LOCALES; ++i)
        {
            row.name[i] = fields[1 + 2 * i].GetCppString();
            row.description[i] = fields[2 + 2 * i].GetCppString();
        }
        return row;
    }

    std::vector<ItemLocaleRow> QueryItemLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                                  "`name_loc1`,`description_loc1`,`name_loc2`,`description_loc2`,"
                                                  "`name_loc3`,`description_loc3`,`name_loc4`,`description_loc4`,"
                                                  "`name_loc5`,`description_loc5`,`name_loc6`,`description_loc6`,"
                                                  "`name_loc7`,`description_loc7`,`name_loc8`,`description_loc8`,"
                                                  "`name_loc9`,`description_loc9`,`name_loc10`,`description_loc10`,"
                                                  "`name_loc11`,`description_loc11`"
                                                  " FROM `locales_item`");
        return ReadRows<ItemLocaleRow>(result, ReadItemLocale);
    }

    static ItemConvertRow ReadItemConvert(Field* fields)
    {
        ItemConvertRow row;
        row.entry = fields[0].GetUInt32();
        row.item = fields[1].GetUInt32();
        return row;
    }

    std::vector<ItemConvertRow> QueryItemConverts()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`item` FROM `item_convert`");
        return ReadRows<ItemConvertRow>(result, ReadItemConvert);
    }

    static ItemExpireConvertRow ReadItemExpireConvert(Field* fields)
    {
        ItemExpireConvertRow row;
        row.entry = fields[0].GetUInt32();
        row.item = fields[1].GetUInt32();
        return row;
    }

    std::vector<ItemExpireConvertRow> QueryItemExpireConverts()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`item` FROM `item_expire_convert`");
        return ReadRows<ItemExpireConvertRow>(result, ReadItemExpireConvert);
    }

    static ItemRequiredTargetRow ReadItemRequiredTarget(Field* fields)
    {
        ItemRequiredTargetRow row;
        row.entry = fields[0].GetUInt32();
        row.type = fields[1].GetUInt32();
        row.targetEntry = fields[2].GetUInt32();
        return row;
    }

    std::vector<ItemRequiredTargetRow> QueryItemRequiredTargets()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,`type`,`targetEntry` FROM `item_required_target`");
        return ReadRows<ItemRequiredTargetRow>(result, ReadItemRequiredTarget);
    }

    static HotfixRow ReadHotfix(Field* fields)
    {
        HotfixRow row;
        row.entry = fields[0].GetUInt32();
        row.type = fields[1].GetUInt32();
        row.timestamp = fields[2].GetUInt32();
        return row;
    }

    std::vector<HotfixRow> QueryHotfixes()
    {
        QueryResult* result = WorldDatabase.Query("SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data");
        return ReadRows<HotfixRow>(result, ReadHotfix);
    }

    QueryResult* QueryItemEntryByName(std::string itemName)
    {
        WorldDatabase.escape_string(itemName);
        return WorldDatabase.PQuery("SELECT `entry` FROM `item_template` WHERE `name` = '%s'", itemName.c_str());
    }
}
