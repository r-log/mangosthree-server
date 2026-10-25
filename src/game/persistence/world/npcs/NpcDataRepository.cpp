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

#include "NpcDataRepository.h"
#include "Database/DatabaseEnv.h"

namespace NpcDataRepository
{
    QueryResult* QueryGossipMenus()
    {
        //                                 0      1        2
        return WorldDatabase.Query("SELECT `entry`, `text_id`, `script_id`, "
                                   //   3
                                   "`condition_id` FROM `gossip_menu`");
    }

    QueryResult* QueryGossipMenuOptions()
    {
        return WorldDatabase.Query("SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, "
                                   "`action_menu_id`, `action_poi_id`, `action_script_id`, `box_coded`, `box_money`, `box_text`, "
                                   "`condition_id` "
                                   "FROM `gossip_menu_option` ORDER BY `menu_id`, `id`");
    }

    QueryResult* QueryNpcTexts()
    {
        return WorldDatabase.Query("SELECT * FROM `npc_text`");
    }

    QueryResult* QueryTrainerSpells(char const* table)
    {
        return WorldDatabase.PQuery("SELECT `entry`, `spell`,`spellcost`,`reqskill`,`reqskillvalue`,`reqlevel` FROM `%s`", table);
    }

    QueryResult* QueryVendorItems(char const* table)
    {
        return WorldDatabase.PQuery("SELECT `entry`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `condition_id` FROM `%s`", table);
    }

    void InsertVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `npc_vendor` (`entry`,`item`,`maxcount`,`incrtime`,`extendedcost`) VALUES('%u','%i','%u','%u','%u')", entry, item, maxcount, incrtime, extendedcost);
    }

    void DeleteVendorItem(uint32 entry, uint32 item)
    {
        WorldDatabase.PExecuteLog("DELETE FROM `npc_vendor` WHERE `entry`='%u' AND `item`='%i'", entry, item);
    }

    QueryResult* QuerySpellClickSpells()
    {
        //                                  0            1           2              3                     4            5             6
        return WorldDatabase.Query("SELECT `npc_entry`, `spell_id`, `quest_start`, `quest_start_active`, `quest_end`, `cast_flags`, `condition_id` FROM `npc_spellclick_spells`");
    }

    QueryResult* QueryPointsOfInterest()
    {
        //                                 0      1  2  3      4     5
        return WorldDatabase.Query("SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`");
    }
}
