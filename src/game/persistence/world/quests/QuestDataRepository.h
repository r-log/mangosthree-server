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

#ifndef MANGOS_H_QUESTDATAREPOSITORY
#define MANGOS_H_QUESTDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

class QueryResult;

/// One `quest_relations` row.
struct QuestRelationRow
{
    uint32 entry = 0;
    uint32 quest = 0;
};

enum
{
    /// The locales a quest locale row holds its texts for: every locale after the default one
    /// (`loc1` to `loc11`).
    QUEST_LOCALE_ROW_LOCALES = 11,
    /// The objective texts of a quest (`ObjectiveText1` to `ObjectiveText4`).
    QUEST_LOCALE_ROW_OBJECTIVES = 4
};

/// One `locales_quest` row: a quest's texts in each locale.
struct QuestLocaleRow
{
    uint32 entry = 0;
    std::string title[QUEST_LOCALE_ROW_LOCALES];
    std::string details[QUEST_LOCALE_ROW_LOCALES];
    std::string objectives[QUEST_LOCALE_ROW_LOCALES];
    std::string offerRewardText[QUEST_LOCALE_ROW_LOCALES];
    std::string requestItemsText[QUEST_LOCALE_ROW_LOCALES];
    std::string endText[QUEST_LOCALE_ROW_LOCALES];
    std::string completedText[QUEST_LOCALE_ROW_LOCALES];
    std::string objectiveText[QUEST_LOCALE_ROW_LOCALES][QUEST_LOCALE_ROW_OBJECTIVES];
    std::string portraitGiverName[QUEST_LOCALE_ROW_LOCALES];
    std::string portraitGiverText[QUEST_LOCALE_ROW_LOCALES];
    std::string portraitTurnInName[QUEST_LOCALE_ROW_LOCALES];
    std::string portraitTurnInText[QUEST_LOCALE_ROW_LOCALES];
};

/// One `quest_poi` row.
struct QuestPoiRow
{
    uint32 questId = 0;
    uint32 poiId = 0;
    int32 objIndex = 0;
    uint32 mapId = 0;
    uint32 mapAreaId = 0;
    uint32 floorId = 0;
    uint32 unk3 = 0;
    uint32 unk4 = 0;
};

/// One `quest_poi_points` row.
struct QuestPoiPointRow
{
    uint32 questId = 0;
    uint32 poiId = 0;
    int32 x = 0;
    int32 y = 0;
};

enum
{
    /// The objectives of a quest: its texts, creatures or game objects and spells to cast.
    QUEST_TEMPLATE_ROW_OBJECTIVES = 4,
    /// The items a quest asks for.
    QUEST_TEMPLATE_ROW_ITEM_OBJECTIVES = 6,
    /// The source items of a quest.
    QUEST_TEMPLATE_ROW_SOURCE_ITEMS = 4,
    /// The currencies a quest asks for.
    QUEST_TEMPLATE_ROW_REQUIRED_CURRENCIES = 4,
    /// The reward items a player chooses one of.
    QUEST_TEMPLATE_ROW_REWARD_CHOICES = 6,
    /// The reward items a player gets every one of.
    QUEST_TEMPLATE_ROW_REWARDS = 4,
    /// The reward currencies.
    QUEST_TEMPLATE_ROW_REWARD_CURRENCIES = 4,
    /// The reward reputations.
    QUEST_TEMPLATE_ROW_REPUTATIONS = 5,
    /// The emotes of a quest's details and offer reward texts.
    QUEST_TEMPLATE_ROW_EMOTES = 4
};

/// One `quest_template` row: every column the quest load reads, each numbered set of columns as
/// an array.
struct QuestTemplateRow
{
    uint32 entry = 0;
    uint32 method = 0;
    int32 zoneOrSort = 0;
    uint32 minLevel = 0;
    int32 questLevel = 0;
    uint32 type = 0;
    uint32 requiredClasses = 0;
    uint32 requiredRaces = 0;
    uint32 requiredSkill = 0;
    uint32 requiredSkillValue = 0;
    uint32 repObjectiveFaction = 0;
    int32 repObjectiveValue = 0;
    uint32 requiredMinRepFaction = 0;
    int32 requiredMinRepValue = 0;
    uint32 requiredMaxRepFaction = 0;
    int32 requiredMaxRepValue = 0;
    uint32 suggestedPlayers = 0;
    uint32 limitTime = 0;
    uint16 questFlags = 0;
    uint16 specialFlags = 0;
    uint32 charTitleId = 0;
    uint32 playersSlain = 0;
    uint32 bonusTalents = 0;
    uint32 portraitGiver = 0;
    uint32 portraitTurnIn = 0;
    int32 prevQuestId = 0;
    int32 nextQuestId = 0;
    int32 exclusiveGroup = 0;
    uint32 nextQuestInChain = 0;
    uint32 rewXPId = 0;
    uint32 srcItemId = 0;
    uint32 srcItemCount = 0;
    uint32 srcSpell = 0;
    std::string title;
    std::string details;
    std::string objectives;
    std::string offerRewardText;
    std::string requestItemsText;
    std::string endText;
    std::string completedText;
    std::string portraitGiverName;
    std::string portraitGiverText;
    std::string portraitTurnInName;
    std::string portraitTurnInText;
    std::string objectiveText[QUEST_TEMPLATE_ROW_OBJECTIVES];
    uint32 reqItemId[QUEST_TEMPLATE_ROW_ITEM_OBJECTIVES] = {};
    uint32 reqItemCount[QUEST_TEMPLATE_ROW_ITEM_OBJECTIVES] = {};
    uint32 reqSourceId[QUEST_TEMPLATE_ROW_SOURCE_ITEMS] = {};
    uint32 reqSourceCount[QUEST_TEMPLATE_ROW_SOURCE_ITEMS] = {};
    int32 reqCreatureOrGOId[QUEST_TEMPLATE_ROW_OBJECTIVES] = {};
    uint32 reqCreatureOrGOCount[QUEST_TEMPLATE_ROW_OBJECTIVES] = {};
    uint32 reqCurrencyId[QUEST_TEMPLATE_ROW_REQUIRED_CURRENCIES] = {};
    uint32 reqCurrencyCount[QUEST_TEMPLATE_ROW_REQUIRED_CURRENCIES] = {};
    uint32 reqSpellCast[QUEST_TEMPLATE_ROW_OBJECTIVES] = {};
    uint32 reqSpellLearned = 0;
    uint32 rewChoiceItemId[QUEST_TEMPLATE_ROW_REWARD_CHOICES] = {};
    uint32 rewChoiceItemCount[QUEST_TEMPLATE_ROW_REWARD_CHOICES] = {};
    uint32 rewItemId[QUEST_TEMPLATE_ROW_REWARDS] = {};
    uint32 rewItemCount[QUEST_TEMPLATE_ROW_REWARDS] = {};
    uint32 rewCurrencyId[QUEST_TEMPLATE_ROW_REWARD_CURRENCIES] = {};
    uint32 rewCurrencyCount[QUEST_TEMPLATE_ROW_REWARD_CURRENCIES] = {};
    uint32 rewSkill = 0;
    uint32 rewSkillValue = 0;
    uint32 rewRepFaction[QUEST_TEMPLATE_ROW_REPUTATIONS] = {};
    int32 rewRepValueId[QUEST_TEMPLATE_ROW_REPUTATIONS] = {};
    int32 rewRepValue[QUEST_TEMPLATE_ROW_REPUTATIONS] = {};
    uint32 rewHonorAddition = 0;
    float rewHonorMultiplier = 0.0f;
    int32 rewOrReqMoney = 0;
    uint32 rewMoneyMaxLevel = 0;
    uint32 rewSpell = 0;
    uint32 rewSpellCast = 0;
    uint32 rewMailTemplateId = 0;
    uint32 rewMailDelaySecs = 0;
    uint32 pointMapId = 0;
    float pointX = 0.0f;
    float pointY = 0.0f;
    uint32 pointOpt = 0;
    uint32 detailsEmote[QUEST_TEMPLATE_ROW_EMOTES] = {};
    uint32 detailsEmoteDelay[QUEST_TEMPLATE_ROW_EMOTES] = {};
    uint32 incompleteEmote = 0;
    uint32 completeEmote = 0;
    int32 offerRewardEmote[QUEST_TEMPLATE_ROW_EMOTES] = {};
    int32 offerRewardEmoteDelay[QUEST_TEMPLATE_ROW_EMOTES] = {};
    uint32 soundAccept = 0;
    uint32 soundTurnIn = 0;
    uint32 startScript = 0;
    uint32 completeScript = 0;
};

/// The quest data on the world database: `ObjectMgr`'s quest, quest relation and quest point of
/// interest loads at startup and on a reload, each run synchronously and read into rows.
namespace QuestDataRepository
{
    /// Selects every quest (`quest_template`).
    std::vector<QuestTemplateRow> QueryQuests();

    /// Selects every quest's localised texts (`locales_quest`).
    std::vector<QuestLocaleRow> QueryQuestLocales();

    /// Selects the quest relations of one actor and role.
    std::vector<QuestRelationRow> QueryQuestRelations(int actor, int role);

    /// Selects every quest relation of one actor, whatever its role.
    std::vector<QuestRelationRow> QueryActorQuestRelations(int actor);

    /// Selects every quest point of interest (`quest_poi`).
    std::vector<QuestPoiRow> QueryQuestPois();

    /// Selects every quest point of interest's points (`quest_poi_points`).
    std::vector<QuestPoiPointRow> QueryQuestPoiPoints();
}

#endif
