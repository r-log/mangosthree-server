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

#include "QuestDataRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace QuestDataRepository
{
    QueryResult* QueryQuests()
    {
        //                                 0      1       2           3         4           5     6                7              8              9
        return WorldDatabase.Query("SELECT `entry`, `Method`, `ZoneOrSort`, `MinLevel`, `QuestLevel`, `Type`, `RequiredClasses`, `RequiredRaces`, `RequiredSkill`, `RequiredSkillValue`,"
                                   //   10                   11                 12                     13                   14                     15                   16                17
                                   "`RepObjectiveFaction`, `RepObjectiveValue`, `RequiredMinRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepFaction`, `RequiredMaxRepValue`, `SuggestedPlayers`, `LimitTime`,"
                                   //   18         19              20             21              22              23               24                25             26             27                28
                                   "`QuestFlags`, `SpecialFlags`, `CharTitleId`, `PlayersSlain`, `BonusTalents`, `PortraitGiver`, `PortraitTurnIn`, `PrevQuestId`, `NextQuestId`, `ExclusiveGroup`, `NextQuestInChain`,"
                                   //   29      30           31              32
                                   "`RewXPId`, `SrcItemId`, `SrcItemCount`, `SrcSpell`,"
                                   //   33    34         35            36                 37                  38         39               40                   41                   42                    43
                                   "`Title`, `Details`, `Objectives`, `OfferRewardText`, `RequestItemsText`, `EndText`, `CompletedText`, `PortraitGiverName`, `PortraitGiverText`, `PortraitTurnInName`, `PortraitTurnInText`,"
                                   //    44            45                46                47
                                   "`ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`,"
                                   //   48         49            50            51            52            53            54               55               56               57               58               59
                                   "`ReqItemId1`, `ReqItemId2`, `ReqItemId3`, `ReqItemId4`, `ReqItemId5`, `ReqItemId6`, `ReqItemCount1`, `ReqItemCount2`, `ReqItemCount3`, `ReqItemCount4`, `ReqItemCount5`, `ReqItemCount6`,"
                                   //   60           61              62              63              64                 65                 66                 67
                                   "`ReqSourceId1`, `ReqSourceId2`, `ReqSourceId3`, `ReqSourceId4`, `ReqSourceCount1`, `ReqSourceCount2`, `ReqSourceCount3`, `ReqSourceCount4`,"
                                   //   68                 69                    70                    71                    72                       73                       74                       75
                                   "`ReqCreatureOrGOId1`, `ReqCreatureOrGOId2`, `ReqCreatureOrGOId3`, `ReqCreatureOrGOId4`, `ReqCreatureOrGOCount1`, `ReqCreatureOrGOCount2`, `ReqCreatureOrGOCount3`, `ReqCreatureOrGOCount4`,"
                                   //   76             77                78                79
                                   "`ReqCurrencyId1`, `ReqCurrencyId2`, `ReqCurrencyId3`, `ReqCurrencyId4`,"
                                   //   80                81                   82                   83
                                   "`ReqCurrencyCount1`, `ReqCurrencyCount2`, `ReqCurrencyCount3`, `ReqCurrencyCount4`,"
                                   //   84            85               86               87               88
                                   "`ReqSpellCast1`, `ReqSpellCast2`, `ReqSpellCast3`, `ReqSpellCast4`, `ReqSpellLearned`,"
                                   //   89               90                  91                  92                  93                  94
                                   "`RewChoiceItemId1`, `RewChoiceItemId2`, `RewChoiceItemId3`, `RewChoiceItemId4`, `RewChoiceItemId5`, `RewChoiceItemId6`,"
                                   //   95                  96                     97                     98                     99                     100
                                   "`RewChoiceItemCount1`, `RewChoiceItemCount2`, `RewChoiceItemCount3`, `RewChoiceItemCount4`, `RewChoiceItemCount5`, `RewChoiceItemCount6`,"
                                   //  101         102           103           104           105              106              107              108
                                   "`RewItemId1`, `RewItemId2`, `RewItemId3`, `RewItemId4`, `RewItemCount1`, `RewItemCount2`, `RewItemCount3`, `RewItemCount4`,"
                                   //  109             110               111               112
                                   "`RewCurrencyId1`, `RewCurrencyId2`, `RewCurrencyId3`, `RewCurrencyId4`,"
                                   //  113                114                  115                  116
                                   "`RewCurrencyCount1`, `RewCurrencyCount2`, `RewCurrencyCount3`, `RewCurrencyCount4`,"
                                   //  117       118
                                   "`RewSkill`, `RewSkillValue`,"
                                   //   119            120               121               122               123
                                   "`RewRepFaction1`, `RewRepFaction2`, `RewRepFaction3`, `RewRepFaction4`, `RewRepFaction5`,"
                                   //   124            125               126               127               128
                                   "`RewRepValueId1`, `RewRepValueId2`, `RewRepValueId3`, `RewRepValueId4`, `RewRepValueId5`,"
                                   //   129          130             131             132             133
                                   "`RewRepValue1`, `RewRepValue2`, `RewRepValue3`, `RewRepValue4`, `RewRepValue5`,"
                                   //   134              135                   136              137                 138         139
                                   "`RewHonorAddition`, `RewHonorMultiplier`, `RewOrReqMoney`, `RewMoneyMaxLevel`, `RewSpell`, `RewSpellCast`,"
                                   //   140               141                 142           143       144       145
                                   "`RewMailTemplateId`, `RewMailDelaySecs`, `PointMapId`, `PointX`, `PointY`, `PointOpt`,"
                                   //   146           147              148              149              150                   151                   152                   153
                                   "`DetailsEmote1`, `DetailsEmote2`, `DetailsEmote3`, `DetailsEmote4`, `DetailsEmoteDelay1`, `DetailsEmoteDelay2`, `DetailsEmoteDelay3`, `DetailsEmoteDelay4`,"
                                   //   154             155              156                  157                  158                  159
                                   "`IncompleteEmote`, `CompleteEmote`, `OfferRewardEmote1`, `OfferRewardEmote2`, `OfferRewardEmote3`, `OfferRewardEmote4`,"
                                   //   160                    161                       162                       163
                                   "`OfferRewardEmoteDelay1`, `OfferRewardEmoteDelay2`, `OfferRewardEmoteDelay3`, `OfferRewardEmoteDelay4`,"
                                   //   164         165            166            167
                                   "`SoundAccept`, `SoundTurnIn`, `StartScript`, `CompleteScript`"
                                   " FROM `quest_template`");
    }

    QueryResult* QueryQuestLocales()
    {
        return WorldDatabase.Query("SELECT `entry`,"
            //                     1             2              3                 4                      5                       6              7                    8                     9                     10                    11                    12                       13                       14                        15
               "`Title_loc1`,`Details_loc1`,`Objectives_loc1`,`OfferRewardText_loc1`,`RequestItemsText_loc1`,`EndText_loc1`,`CompletedText_loc1`,`ObjectiveText1_loc1`,`ObjectiveText2_loc1`,`ObjectiveText3_loc1`,`ObjectiveText4_loc1`,`PortraitGiverName_loc1`,`PortraitGiverText_loc1`,`PortraitTurnInName_loc1`,`PortraitTurnInText_loc1`,"
               "`Title_loc2`,`Details_loc2`,`Objectives_loc2`,`OfferRewardText_loc2`,`RequestItemsText_loc2`,`EndText_loc2`,`CompletedText_loc2`,`ObjectiveText1_loc2`,`ObjectiveText2_loc2`,`ObjectiveText3_loc2`,`ObjectiveText4_loc2`,`PortraitGiverName_loc2`,`PortraitGiverText_loc2`,`PortraitTurnInName_loc2`,`PortraitTurnInText_loc2`,"
               "`Title_loc3`,`Details_loc3`,`Objectives_loc3`,`OfferRewardText_loc3`,`RequestItemsText_loc3`,`EndText_loc3`,`CompletedText_loc3`,`ObjectiveText1_loc3`,`ObjectiveText2_loc3`,`ObjectiveText3_loc3`,`ObjectiveText4_loc3`,`PortraitGiverName_loc3`,`PortraitGiverText_loc3`,`PortraitTurnInName_loc3`,`PortraitTurnInText_loc3`,"
               "`Title_loc4`,`Details_loc4`,`Objectives_loc4`,`OfferRewardText_loc4`,`RequestItemsText_loc4`,`EndText_loc4`,`CompletedText_loc4`,`ObjectiveText1_loc4`,`ObjectiveText2_loc4`,`ObjectiveText3_loc4`,`ObjectiveText4_loc4`,`PortraitGiverName_loc4`,`PortraitGiverText_loc4`,`PortraitTurnInName_loc4`,`PortraitTurnInText_loc4`,"
               "`Title_loc5`,`Details_loc5`,`Objectives_loc5`,`OfferRewardText_loc5`,`RequestItemsText_loc5`,`EndText_loc5`,`CompletedText_loc5`,`ObjectiveText1_loc5`,`ObjectiveText2_loc5`,`ObjectiveText3_loc5`,`ObjectiveText4_loc5`,`PortraitGiverName_loc5`,`PortraitGiverText_loc5`,`PortraitTurnInName_loc5`,`PortraitTurnInText_loc5`,"
               "`Title_loc6`,`Details_loc6`,`Objectives_loc6`,`OfferRewardText_loc6`,`RequestItemsText_loc6`,`EndText_loc6`,`CompletedText_loc6`,`ObjectiveText1_loc6`,`ObjectiveText2_loc6`,`ObjectiveText3_loc6`,`ObjectiveText4_loc6`,`PortraitGiverName_loc6`,`PortraitGiverText_loc6`,`PortraitTurnInName_loc6`,`PortraitTurnInText_loc6`,"
               "`Title_loc7`,`Details_loc7`,`Objectives_loc7`,`OfferRewardText_loc7`,`RequestItemsText_loc7`,`EndText_loc7`,`CompletedText_loc7`,`ObjectiveText1_loc7`,`ObjectiveText2_loc7`,`ObjectiveText3_loc7`,`ObjectiveText4_loc7`,`PortraitGiverName_loc7`,`PortraitGiverText_loc7`,`PortraitTurnInName_loc7`,`PortraitTurnInText_loc7`,"
               "`Title_loc8`,`Details_loc8`,`Objectives_loc8`,`OfferRewardText_loc8`,`RequestItemsText_loc8`,`EndText_loc8`,`CompletedText_loc8`,`ObjectiveText1_loc8`,`ObjectiveText2_loc8`,`ObjectiveText3_loc8`,`ObjectiveText4_loc8`,`PortraitGiverName_loc8`,`PortraitGiverText_loc8`,`PortraitTurnInName_loc8`,`PortraitTurnInText_loc8`,"
               "`Title_loc9`,`Details_loc9`,`Objectives_loc9`,`OfferRewardText_loc9`,`RequestItemsText_loc9`,`EndText_loc9`,`CompletedText_loc9`,`ObjectiveText1_loc9`,`ObjectiveText2_loc9`,`ObjectiveText3_loc9`,`ObjectiveText4_loc9`,`PortraitGiverName_loc9`,`PortraitGiverText_loc9`,`PortraitTurnInName_loc9`,`PortraitTurnInText_loc9`,"
               "`Title_loc10`,`Details_loc10`,`Objectives_loc10`,`OfferRewardText_loc10`,`RequestItemsText_loc10`,`EndText_loc10`,`CompletedText_loc10`,`ObjectiveText1_loc10`,`ObjectiveText2_loc10`,`ObjectiveText3_loc10`,`ObjectiveText4_loc10`,`PortraitGiverName_loc10`,`PortraitGiverText_loc10`,`PortraitTurnInName_loc10`,`PortraitTurnInText_loc10`,"
               "`Title_loc11`,`Details_loc11`,`Objectives_loc11`,`OfferRewardText_loc11`,`RequestItemsText_loc11`,`EndText_loc11`,`CompletedText_loc11`,`ObjectiveText1_loc11`,`ObjectiveText2_loc11`,`ObjectiveText3_loc11`,`ObjectiveText4_loc11`,`PortraitGiverName_loc11`,`PortraitGiverText_loc11`,`PortraitTurnInName_loc11`,`PortraitTurnInText_loc11`"
               " FROM `locales_quest`"
                                  );
    }

    static QuestRelationRow ReadQuestRelation(Field* fields)
    {
        QuestRelationRow row;
        row.entry = fields[0].GetUInt32();
        row.quest = fields[1].GetUInt32();
        return row;
    }

    std::vector<QuestRelationRow> QueryQuestRelations(int actor, int role)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d AND `role` = %d", actor, role);
        return ReadRows<QuestRelationRow>(result, ReadQuestRelation);
    }

    std::vector<QuestRelationRow> QueryActorQuestRelations(int actor)
    {
        QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d", actor);
        return ReadRows<QuestRelationRow>(result, ReadQuestRelation);
    }

    QueryResult* QueryQuestPois()
    {
        //                                 0        1      2         3      4          5        6     7
        return WorldDatabase.Query("SELECT `questId`, `poiId`, `objIndex`, `mapId`, `mapAreaId`, `floorId`, `unk3`, `unk4` FROM `quest_poi`");
    }

    QueryResult* QueryQuestPoiPoints()
    {
        return WorldDatabase.Query("SELECT `questId`, `poiId`, `x`, `y` FROM `quest_poi_points`");
    }
}
