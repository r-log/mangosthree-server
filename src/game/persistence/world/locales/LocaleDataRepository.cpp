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

#include "LocaleDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace LocaleDataRepository
{
    static CreatureLocaleRow ReadCreatureLocale(Field* fields)
    {
        CreatureLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            row.name[i] = fields[1 + 2 * i].GetCppString();
            row.subname[i] = fields[2 + 2 * i].GetCppString();
        }
        return row;
    }

    std::vector<CreatureLocaleRow> QueryCreatureLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                                  "`name_loc1`,`subname_loc1`,`name_loc2`,`subname_loc2`,"
                                                  "`name_loc3`,`subname_loc3`,`name_loc4`,`subname_loc4`,"
                                                  "`name_loc5`,`subname_loc5`,`name_loc6`,`subname_loc6`,"
                                                  "`name_loc7`,`subname_loc7`,`name_loc8`,`subname_loc8`,"
                                                  "`name_loc9`,`subname_loc9`,`name_loc10`,`subname_loc10`,"
                                                  "`name_loc11`,`subname_loc11`"
                                                  " FROM `locales_creature`");
        return ReadRows<CreatureLocaleRow>(result, ReadCreatureLocale);
    }

    static GossipMenuOptionLocaleRow ReadGossipMenuOptionLocale(Field* fields)
    {
        GossipMenuOptionLocaleRow row;
        row.menuId = fields[0].GetUInt16();
        row.id = fields[1].GetUInt16();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            row.optionText[i] = fields[2 + 2 * i].GetCppString();
            row.boxText[i] = fields[3 + 2 * i].GetCppString();
        }
        return row;
    }

    std::vector<GossipMenuOptionLocaleRow> QueryGossipMenuOptionLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `menu_id`,`id`,"
                                                  "`option_text_loc1`,`box_text_loc1`,`option_text_loc2`,`box_text_loc2`,"
                                                  "`option_text_loc3`,`box_text_loc3`,`option_text_loc4`,`box_text_loc4`,"
                                                  "`option_text_loc5`,`box_text_loc5`,`option_text_loc6`,`box_text_loc6`,"
                                                  "`option_text_loc7`,`box_text_loc7`,`option_text_loc8`,`box_text_loc8`,"
                                                  "`option_text_loc9`,`box_text_loc9`,`option_text_loc10`,`box_text_loc10`,"
                                                  "`option_text_loc11`,`box_text_loc11` "
                                                  "FROM `locales_gossip_menu_option`");
        return ReadRows<GossipMenuOptionLocaleRow>(result, ReadGossipMenuOptionLocale);
    }

    static PointOfInterestLocaleRow ReadPointOfInterestLocale(Field* fields)
    {
        PointOfInterestLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            row.iconName[i] = fields[1 + i].GetCppString();
        }
        return row;
    }

    std::vector<PointOfInterestLocaleRow> QueryPointOfInterestLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                     "`icon_name_loc1`,`icon_name_loc2`,`icon_name_loc3`,`icon_name_loc4`,"
                                     "`icon_name_loc5`,`icon_name_loc6`,`icon_name_loc7`,`icon_name_loc8`,"
                                     "`icon_name_loc9`,`icon_name_loc10`,`icon_name_loc11`"
                                     " FROM `locales_points_of_interest`");
        return ReadRows<PointOfInterestLocaleRow>(result, ReadPointOfInterestLocale);
    }

    static PageTextLocaleRow ReadPageTextLocale(Field* fields)
    {
        PageTextLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            row.text[i] = fields[1 + i].GetCppString();
        }
        return row;
    }

    std::vector<PageTextLocaleRow> QueryPageTextLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                                  "`text_loc1`,`text_loc2`,`text_loc3`,`text_loc4`,"
                                                  "`text_loc5`,`text_loc6`,`text_loc7`,`text_loc8`,"
                                                  "`text_loc9`,`text_loc10`,`text_loc11`"
                                                  " FROM `locales_page_text`");
        return ReadRows<PageTextLocaleRow>(result, ReadPageTextLocale);
    }

    static NpcTextLocaleRow ReadNpcTextLocale(Field* fields)
    {
        NpcTextLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            for (int j = 0; j < NPC_TEXT_LOCALE_ROW_TEXTS; ++j)
            {
                row.text0[i][j] = fields[1 + 2 * NPC_TEXT_LOCALE_ROW_TEXTS * i + 2 * j].GetCppString();
                row.text1[i][j] = fields[2 + 2 * NPC_TEXT_LOCALE_ROW_TEXTS * i + 2 * j].GetCppString();
            }
        }
        return row;
    }

    std::vector<NpcTextLocaleRow> QueryNpcTextLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                                  "`Text0_0_loc1`,`Text0_1_loc1`,`Text1_0_loc1`,`Text1_1_loc1`,`Text2_0_loc1`,`Text2_1_loc1`,`Text3_0_loc1`,`Text3_1_loc1`,`Text4_0_loc1`,`Text4_1_loc1`,`Text5_0_loc1`,`Text5_1_loc1`,`Text6_0_loc1`,`Text6_1_loc1`,`Text7_0_loc1`,`Text7_1_loc1`,"
                                                  "`Text0_0_loc2`,`Text0_1_loc2`,`Text1_0_loc2`,`Text1_1_loc2`,`Text2_0_loc2`,`Text2_1_loc2`,`Text3_0_loc2`,`Text3_1_loc2`,`Text4_0_loc2`,`Text4_1_loc2`,`Text5_0_loc2`,`Text5_1_loc2`,`Text6_0_loc2`,`Text6_1_loc2`,`Text7_0_loc2`,`Text7_1_loc2`,"
                                                  "`Text0_0_loc3`,`Text0_1_loc3`,`Text1_0_loc3`,`Text1_1_loc3`,`Text2_0_loc3`,`Text2_1_loc3`,`Text3_0_loc3`,`Text3_1_loc3`,`Text4_0_loc3`,`Text4_1_loc3`,`Text5_0_loc3`,`Text5_1_loc3`,`Text6_0_loc3`,`Text6_1_loc3`,`Text7_0_loc3`,`Text7_1_loc3`,"
                                                  "`Text0_0_loc4`,`Text0_1_loc4`,`Text1_0_loc4`,`Text1_1_loc4`,`Text2_0_loc4`,`Text2_1_loc4`,`Text3_0_loc4`,`Text3_1_loc4`,`Text4_0_loc4`,`Text4_1_loc4`,`Text5_0_loc4`,`Text5_1_loc4`,`Text6_0_loc4`,`Text6_1_loc4`,`Text7_0_loc4`,`Text7_1_loc4`,"
                                                  "`Text0_0_loc5`,`Text0_1_loc5`,`Text1_0_loc5`,`Text1_1_loc5`,`Text2_0_loc5`,`Text2_1_loc5`,`Text3_0_loc5`,`Text3_1_loc5`,`Text4_0_loc5`,`Text4_1_loc5`,`Text5_0_loc5`,`Text5_1_loc5`,`Text6_0_loc5`,`Text6_1_loc5`,`Text7_0_loc5`,`Text7_1_loc5`,"
                                                  "`Text0_0_loc6`,`Text0_1_loc6`,`Text1_0_loc6`,`Text1_1_loc6`,`Text2_0_loc6`,`Text2_1_loc6`,`Text3_0_loc6`,`Text3_1_loc6`,`Text4_0_loc6`,`Text4_1_loc6`,`Text5_0_loc6`,`Text5_1_loc6`,`Text6_0_loc6`,`Text6_1_loc6`,`Text7_0_loc6`,`Text7_1_loc6`,"
                                                  "`Text0_0_loc7`,`Text0_1_loc7`,`Text1_0_loc7`,`Text1_1_loc7`,`Text2_0_loc7`,`Text2_1_loc7`,`Text3_0_loc7`,`Text3_1_loc7`,`Text4_0_loc7`,`Text4_1_loc7`,`Text5_0_loc7`,`Text5_1_loc7`,`Text6_0_loc7`,`Text6_1_loc7`,`Text7_0_loc7`,`Text7_1_loc7`, "
                                                  "`Text0_0_loc8`,`Text0_1_loc8`,`Text1_0_loc8`,`Text1_1_loc8`,`Text2_0_loc8`,`Text2_1_loc8`,`Text3_0_loc8`,`Text3_1_loc8`,`Text4_0_loc8`,`Text4_1_loc8`,`Text5_0_loc8`,`Text5_1_loc8`,`Text6_0_loc8`,`Text6_1_loc8`,`Text7_0_loc8`,`Text7_1_loc8`,"
                                                  "`Text0_0_loc9`,`Text0_1_loc9`,`Text1_0_loc9`,`Text1_1_loc9`,`Text2_0_loc9`,`Text2_1_loc9`,`Text3_0_loc9`,`Text3_1_loc9`,`Text4_0_loc9`,`Text4_1_loc9`,`Text5_0_loc9`,`Text5_1_loc9`,`Text6_0_loc9`,`Text6_1_loc9`,`Text7_0_loc9`,`Text7_1_loc9`,"
                                                  "`Text0_0_loc10`,`Text0_1_loc10`,`Text1_0_loc10`,`Text1_1_loc10`,`Text2_0_loc10`,`Text2_1_loc10`,`Text3_0_loc10`,`Text3_1_loc10`,`Text4_0_loc10`,`Text4_1_loc10`,`Text5_0_loc10`,`Text5_1_loc10`,`Text6_0_loc10`,`Text6_1_loc10`,`Text7_0_loc10`,`Text7_1_loc10`,"
                                                  "`Text0_0_loc11`,`Text0_1_loc11`,`Text1_0_loc11`,`Text1_1_loc11`,`Text2_0_loc11`,`Text2_1_loc11`,`Text3_0_loc11`,`Text3_1_loc11`,`Text4_0_loc11`,`Text4_1_loc11`,`Text5_0_loc11`,`Text5_1_loc11`,`Text6_0_loc11`,`Text6_1_loc11`,`Text7_0_loc11`,`Text7_1_loc11` "
                                                  " FROM `locales_npc_text`");
        return ReadRows<NpcTextLocaleRow>(result, ReadNpcTextLocale);
    }

    static GameObjectLocaleRow ReadGameObjectLocale(Field* fields)
    {
        GameObjectLocaleRow row;
        row.entry = fields[0].GetUInt32();
        for (int i = 0; i < LOCALE_ROW_LOCALES; ++i)
        {
            row.name[i] = fields[1 + i].GetCppString();
            row.castBarCaption[i] = fields[1 + LOCALE_ROW_LOCALES + i].GetCppString();
        }
        return row;
    }

    std::vector<GameObjectLocaleRow> QueryGameObjectLocales()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                                                  "`name_loc1`,`name_loc2`,`name_loc3`,`name_loc4`,"
                                                  "`name_loc5`,`name_loc6`,`name_loc7`,`name_loc8`,"
                                                  "`name_loc9`,`name_loc10`,`name_loc11`,"
                                                  "`castbarcaption_loc1`,`castbarcaption_loc2`,"
                                                  "`castbarcaption_loc3`,`castbarcaption_loc4`,"
                                                  "`castbarcaption_loc5`,`castbarcaption_loc6`,"
                                                  "`castbarcaption_loc7`,`castbarcaption_loc8`,"
                                                  "`castbarcaption_loc9`,`castbarcaption_loc10`,"
                                                  "`castbarcaption_loc11`"
                                                  " FROM `locales_gameobject`");
        return ReadRows<GameObjectLocaleRow>(result, ReadGameObjectLocale);
    }

    std::vector<StringRow> QueryStrings(Database& db, char const* table, bool extraContent)
    {
        QueryResult* result = db.PQuery("SELECT `entry`,`content_default`,"
                                        "`content_loc1`,`content_loc2`,`content_loc3`,`content_loc4`,"
                                        "`content_loc5`,`content_loc6`,`content_loc7`,`content_loc8`,"
                                        "`content_loc9`,`content_loc10`,`content_loc11`"
                                        " %s FROM %s", extraContent ? ",sound,type,language,emote" : "", table);
        return ReadRows<StringRow>(result, [extraContent](Field* fields)
                                   {
                                       StringRow row;
                                       row.entry = fields[0].GetInt32();
                                       row.contentDefault = fields[1].GetCppString();
                                       for (int i = 0; i < STRING_ROW_LOCALES; ++i)
                                       {
                                           row.content[i] = fields[i + 2].GetCppString();
                                       }
                                       if (extraContent)
                                       {
                                           row.sound = fields[13].GetUInt32();
                                           row.type = fields[14].GetUInt32();
                                           row.language = fields[15].GetUInt32();
                                           row.emote = fields[16].GetUInt32();
                                       }
                                       return row;
                                   });
    }
}
