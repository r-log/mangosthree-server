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
#include "persistence/RowReader.h"

namespace NpcDataRepository
{
    static GossipMenuRow ReadGossipMenu(Field* fields)
    {
        GossipMenuRow row;
        row.entry = fields[0].GetUInt32();
        row.textId = fields[1].GetUInt32();
        row.scriptId = fields[2].GetUInt32();
        row.conditionId = fields[3].GetUInt16();
        return row;
    }

    std::vector<GossipMenuRow> QueryGossipMenus()
    {
        //                                 0      1        2
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `text_id`, `script_id`, "
                                                  //   3
                                                  "`condition_id` FROM `gossip_menu`");
        return ReadRows<GossipMenuRow>(result, ReadGossipMenu);
    }

    static GossipMenuOptionRow ReadGossipMenuOption(Field* fields)
    {
        GossipMenuOptionRow row;
        row.menuId = fields[0].GetUInt32();
        row.id = fields[1].GetUInt32();
        row.optionIcon = fields[2].GetUInt8();
        row.optionText = fields[3].GetCppString();
        row.optionId = fields[4].GetUInt32();
        row.npcOptionNpcflag = fields[5].GetUInt32();
        row.actionMenuId = fields[6].GetInt32();
        row.actionPoiId = fields[7].GetUInt32();
        row.actionScriptId = fields[8].GetUInt32();
        row.boxCoded = fields[9].GetUInt8();
        row.boxMoney = fields[10].GetUInt32();
        row.boxText = fields[11].GetCppString();
        row.conditionId = fields[12].GetUInt16();
        return row;
    }

    std::vector<GossipMenuOptionRow> QueryGossipMenuOptions()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, "
                                                  "`action_menu_id`, `action_poi_id`, `action_script_id`, `box_coded`, `box_money`, `box_text`, "
                                                  "`condition_id` "
                                                  "FROM `gossip_menu_option` ORDER BY `menu_id`, `id`");
        return ReadRows<GossipMenuOptionRow>(result, ReadGossipMenuOption);
    }

    static NpcTextRow ReadNpcText(Field* fields)
    {
        NpcTextRow row;
        int column = 0;
        row.id = fields[column++].GetUInt32();
        for (int i = 0; i < NPC_TEXT_ROW_OPTIONS; ++i)
        {
            NpcTextOptionRow& option = row.options[i];
            option.text0 = fields[column++].GetCppString();
            option.text1 = fields[column++].GetCppString();
            option.language = fields[column++].GetUInt32();
            option.probability = fields[column++].GetFloat();
            for (int j = 0; j < NPC_TEXT_ROW_EMOTES; ++j)
            {
                option.emoteDelay[j] = fields[column++].GetUInt32();
                option.emote[j] = fields[column++].GetUInt32();
            }
        }
        return row;
    }

    std::vector<NpcTextRow> QueryNpcTexts()
    {
        QueryResult* result = WorldDatabase.Query("SELECT * FROM `npc_text`");
        return ReadRows<NpcTextRow>(result, ReadNpcText);
    }

    static TrainerSpellRow ReadTrainerSpell(Field* fields)
    {
        TrainerSpellRow row;
        row.entry = fields[0].GetUInt32();
        row.spell = fields[1].GetUInt32();
        row.spellcost = fields[2].GetUInt32();
        row.reqskill = fields[3].GetUInt32();
        row.reqskillvalue = fields[4].GetUInt32();
        row.reqlevel = fields[5].GetUInt32();
        return row;
    }

    std::vector<TrainerSpellRow> QueryTrainerSpells(char const* table)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `spell`,`spellcost`,`reqskill`,`reqskillvalue`,`reqlevel` FROM `%s`", table);
        return ReadRows<TrainerSpellRow>(result, ReadTrainerSpell);
    }

    static VendorItemRow ReadVendorItem(Field* fields)
    {
        VendorItemRow row;
        row.entry = fields[0].GetUInt32();
        row.item = fields[1].GetInt32();
        row.maxcount = fields[2].GetUInt32();
        row.incrtime = fields[3].GetUInt32();
        row.extendedCost = fields[4].GetUInt32();
        row.conditionId = fields[5].GetUInt16();
        return row;
    }

    std::vector<VendorItemRow> QueryVendorItems(char const* table)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `item`, `maxcount`, `incrtime`, `ExtendedCost`, `condition_id` FROM `%s`", table);
        return ReadRows<VendorItemRow>(result, ReadVendorItem);
    }

    void InsertVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime, uint32 extendedcost)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `npc_vendor` (`entry`,`item`,`maxcount`,`incrtime`,`extendedcost`) VALUES('%u','%i','%u','%u','%u')", entry, item, maxcount, incrtime, extendedcost);
    }

    void DeleteVendorItem(uint32 entry, uint32 item)
    {
        WorldDatabase.PExecuteLog("DELETE FROM `npc_vendor` WHERE `entry`='%u' AND `item`='%i'", entry, item);
    }

    static SpellClickSpellRow ReadSpellClickSpell(Field* fields)
    {
        SpellClickSpellRow row;
        row.npcEntry = fields[0].GetUInt32();
        row.spellId = fields[1].GetUInt32();
        row.questStart = fields[2].GetUInt32();
        row.questStartActive = fields[3].GetBool();
        row.questEnd = fields[4].GetUInt32();
        row.castFlags = fields[5].GetUInt8();
        row.conditionId = fields[6].GetUInt16();
        return row;
    }

    std::vector<SpellClickSpellRow> QuerySpellClickSpells()
    {
        //                                  0            1           2              3                     4            5             6
        QueryResult* result = WorldDatabase.Query("SELECT `npc_entry`, `spell_id`, `quest_start`, `quest_start_active`, `quest_end`, `cast_flags`, `condition_id` FROM `npc_spellclick_spells`");
        return ReadRows<SpellClickSpellRow>(result, ReadSpellClickSpell);
    }

    static PointOfInterestRow ReadPointOfInterest(Field* fields)
    {
        PointOfInterestRow row;
        row.entry = fields[0].GetUInt32();
        row.x = fields[1].GetFloat();
        row.y = fields[2].GetFloat();
        row.icon = fields[3].GetUInt32();
        row.flags = fields[4].GetUInt32();
        row.data = fields[5].GetUInt32();
        row.iconName = fields[6].GetCppString();
        return row;
    }

    std::vector<PointOfInterestRow> QueryPointsOfInterest()
    {
        //                                 0      1  2  3      4     5
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`");
        return ReadRows<PointOfInterestRow>(result, ReadPointOfInterest);
    }

    QueryResult* QueryVendorItemEntries()
    {
        return WorldDatabase.Query("SELECT DISTINCT `item` FROM `npc_vendor`");
    }
}
