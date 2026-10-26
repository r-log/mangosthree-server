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

namespace ItemDataRepository
{
    QueryResult* QueryItemLocales()
    {
        return WorldDatabase.Query("SELECT `entry`,"
                                   "`name_loc1`,`description_loc1`,`name_loc2`,`description_loc2`,"
                                   "`name_loc3`,`description_loc3`,`name_loc4`,`description_loc4`,"
                                   "`name_loc5`,`description_loc5`,`name_loc6`,`description_loc6`,"
                                   "`name_loc7`,`description_loc7`,`name_loc8`,`description_loc8`,"
                                   "`name_loc9`,`description_loc9`,`name_loc10`,`description_loc10`,"
                                   "`name_loc11`,`description_loc11`"
                                   " FROM `locales_item`");
    }

    QueryResult* QueryItemConverts()
    {
        return WorldDatabase.Query("SELECT `entry`,`item` FROM `item_convert`");
    }

    QueryResult* QueryItemExpireConverts()
    {
        return WorldDatabase.Query("SELECT `entry`,`item` FROM `item_expire_convert`");
    }

    QueryResult* QueryItemRequiredTargets()
    {
        return WorldDatabase.Query("SELECT `entry`,`type`,`targetEntry` FROM `item_required_target`");
    }

    QueryResult* QueryHotfixes()
    {
        return WorldDatabase.Query("SELECT entry, type, UNIX_TIMESTAMP(hotfixDate) FROM hotfix_data");
    }

    QueryResult* QueryItemEntryByName(std::string itemName)
    {
        WorldDatabase.escape_string(itemName);
        return WorldDatabase.PQuery("SELECT `entry` FROM `item_template` WHERE `name` = '%s'", itemName.c_str());
    }
}
