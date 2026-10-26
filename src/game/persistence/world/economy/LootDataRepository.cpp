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

#include "LootDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace LootDataRepository
{
    static LootTemplateRow ReadLootTemplate(Field* fields)
    {
        LootTemplateRow row;
        row.entry = fields[0].GetUInt32();
        row.item = fields[1].GetInt32();
        row.chanceOrQuestChance = fields[2].GetFloat();
        row.groupid = fields[3].GetUInt8();
        row.mincountOrRef = fields[4].GetInt32();
        row.maxcount = fields[5].GetUInt32();
        row.conditionId = fields[6].GetUInt16();
        return row;
    }

    std::vector<LootTemplateRow> QueryLootTemplate(char const* table)
    {
        //                                  0      1     2                    3        4              5         6
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `item`, `ChanceOrQuestChance`, `groupid`, `mincountOrRef`, `maxcount`, `condition_id` FROM `%s`", table);
        return ReadRows<LootTemplateRow>(result, ReadLootTemplate);
    }

    static ItemEnchantmentRow ReadItemEnchantment(Field* fields)
    {
        ItemEnchantmentRow row;
        row.entry = fields[0].GetInt32();
        row.ench = fields[1].GetUInt32();
        row.chance = fields[2].GetFloat();
        return row;
    }

    std::vector<ItemEnchantmentRow> QueryItemEnchantments()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `ench`, `chance` FROM `item_enchantment_template`");
        return ReadRows<ItemEnchantmentRow>(result, ReadItemEnchantment);
    }

    QueryResult* QueryLootItemEntries()
    {
        return WorldDatabase.PQuery("SELECT `item` FROM `creature_loot_template` UNION "
                                    "SELECT `item` FROM `disenchant_loot_template` UNION "
                                    "SELECT `item` FROM `fishing_loot_template` UNION "
                                    "SELECT `item` FROM `gameobject_loot_template` UNION "
                                    "SELECT `item` FROM `item_loot_template` UNION "
                                    "SELECT `item` FROM `milling_loot_template` UNION "
                                    "SELECT `item` FROM `pickpocketing_loot_template` UNION "
                                    "SELECT `item` FROM `prospecting_loot_template` UNION "
                                    "SELECT `item` FROM `skinning_loot_template` UNION "
                                    "SELECT `item` FROM `spell_loot_template`");
    }
}
