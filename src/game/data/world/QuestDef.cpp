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

/**
 * @file QuestDef.cpp
 * @brief Quest data structure implementation
 *
 * This file implements the Quest class which represents quest data
 * loaded from the quest database tables. Features:
 *
 * - Quest data loading from database
 * - Quest requirement validation
 * - Quest reward calculation
 * - Quest objective checking
 * - Quest condition evaluation
 *
 * Quest data includes requirements, objectives, rewards, and
 * conditions that determine quest eligibility and completion.
 *
 * @see Quest for the quest class
 * @see QuestMgr for quest management
 */

#include "QuestDef.h"
#include "Database/DatabaseEnv.h"
#include "data/GameConfig.h"
#include "DBCStores.h"
#include "persistence/world/quests/QuestDataRepository.h"

static_assert(int(QUEST_TEMPLATE_ROW_OBJECTIVES) == int(QUEST_OBJECTIVES_COUNT), "a quest template row holds every objective");
static_assert(int(QUEST_TEMPLATE_ROW_ITEM_OBJECTIVES) == int(QUEST_ITEM_OBJECTIVES_COUNT), "a quest template row holds every item objective");
static_assert(int(QUEST_TEMPLATE_ROW_SOURCE_ITEMS) == int(QUEST_SOURCE_ITEM_IDS_COUNT), "a quest template row holds every source item");
static_assert(int(QUEST_TEMPLATE_ROW_REQUIRED_CURRENCIES) == int(QUEST_REQUIRED_CURRENCY_COUNT), "a quest template row holds every required currency");
static_assert(int(QUEST_TEMPLATE_ROW_REWARD_CHOICES) == int(QUEST_REWARD_CHOICES_COUNT), "a quest template row holds every reward choice");
static_assert(int(QUEST_TEMPLATE_ROW_REWARDS) == int(QUEST_REWARDS_COUNT), "a quest template row holds every reward item");
static_assert(int(QUEST_TEMPLATE_ROW_REWARD_CURRENCIES) == int(QUEST_REWARD_CURRENCY_COUNT), "a quest template row holds every reward currency");
static_assert(int(QUEST_TEMPLATE_ROW_REPUTATIONS) == int(QUEST_REPUTATIONS_COUNT), "a quest template row holds every reward reputation");
static_assert(int(QUEST_TEMPLATE_ROW_EMOTES) == int(QUEST_EMOTE_COUNT), "a quest template row holds every emote");

Quest::Quest(QuestTemplateRow const& row)
{
    QuestId = row.entry;
    QuestMethod = row.method;
    ZoneOrSort = row.zoneOrSort;
    MinLevel = row.minLevel;
    QuestLevel = row.questLevel;
    Type = row.type;
    RequiredClasses = row.requiredClasses;
    RequiredRaces = row.requiredRaces;
    RequiredSkill = row.requiredSkill;
    RequiredSkillValue = row.requiredSkillValue;
    RepObjectiveFaction = row.repObjectiveFaction;
    RepObjectiveValue = row.repObjectiveValue;
    RequiredMinRepFaction = row.requiredMinRepFaction;
    RequiredMinRepValue = row.requiredMinRepValue;
    RequiredMaxRepFaction = row.requiredMaxRepFaction;
    RequiredMaxRepValue = row.requiredMaxRepValue;
    SuggestedPlayers = row.suggestedPlayers;
    LimitTime = row.limitTime;
    m_QuestFlags = row.questFlags;
    m_SpecialFlags = row.specialFlags;
    CharTitleId = row.charTitleId;
    PlayersSlain = row.playersSlain;
    BonusTalents = row.bonusTalents;

    PortraitGiver = row.portraitGiver;
    PortraitTurnIn = row.portraitTurnIn;

    PrevQuestId = row.prevQuestId;
    NextQuestId = row.nextQuestId;
    ExclusiveGroup = row.exclusiveGroup;
    NextQuestInChain = row.nextQuestInChain;
    RewXPId = row.rewXPId;
    SrcItemId = row.srcItemId;
    SrcItemCount = row.srcItemCount;
    SrcSpell = row.srcSpell;
    Title = row.title;
    Details = row.details;
    Objectives = row.objectives;
    OfferRewardText = row.offerRewardText;
    RequestItemsText = row.requestItemsText;
    EndText = row.endText;
    CompletedText = row.completedText;

    PortraitGiverName = row.portraitGiverName;
    PortraitGiverText = row.portraitGiverText;
    PortraitTurnInName = row.portraitTurnInName;
    PortraitTurnInText = row.portraitTurnInText;

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ObjectiveText[i] = row.objectiveText[i];
    }

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        ReqItemId[i] = row.reqItemId[i];
    }

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        ReqItemCount[i] = row.reqItemCount[i];
    }

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
    {
        ReqSourceId[i] = row.reqSourceId[i];
    }

    for (int i = 0; i < QUEST_SOURCE_ITEM_IDS_COUNT; ++i)
    {
        ReqSourceCount[i] = row.reqSourceCount[i];
    }

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ReqCreatureOrGOId[i] = row.reqCreatureOrGOId[i];
    }

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ReqCreatureOrGOCount[i] = row.reqCreatureOrGOCount[i];
    }

    for (int i = 0; i < QUEST_REQUIRED_CURRENCY_COUNT; ++i)
    {
        ReqCurrencyId[i] = row.reqCurrencyId[i];
    }
    for (int i = 0; i < QUEST_REQUIRED_CURRENCY_COUNT; ++i)
    {
        ReqCurrencyCount[i] = row.reqCurrencyCount[i];
    }

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        ReqSpell[i] = row.reqSpellCast[i];
    }

    ReqSpellLearned = row.reqSpellLearned;


    for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        RewChoiceItemId[i] = row.rewChoiceItemId[i];
    }

    for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        RewChoiceItemCount[i] = row.rewChoiceItemCount[i];
    }

    for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        RewItemId[i] = row.rewItemId[i];
    }

    for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        RewItemCount[i] = row.rewItemCount[i];
    }

    for (int i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        RewCurrencyId[i] = row.rewCurrencyId[i];
    }
    for (int i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
    {
        RewCurrencyCount[i] = row.rewCurrencyCount[i];
    }

    RewSkill = row.rewSkill;
    RewSkillValue = row.rewSkillValue;

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        RewRepFaction[i] = row.rewRepFaction[i];
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        RewRepValueId[i] = row.rewRepValueId[i];
    }

    for (int i = 0; i < QUEST_REPUTATIONS_COUNT; ++i)
    {
        RewRepValue[i] = row.rewRepValue[i];
    }

    RewHonorAddition = row.rewHonorAddition;
    RewHonorMultiplier = row.rewHonorMultiplier;
    RewOrReqMoney = row.rewOrReqMoney;
    RewMoneyMaxLevel = row.rewMoneyMaxLevel;
    RewSpell = row.rewSpell;
    RewSpellCast = row.rewSpellCast;
    RewMailTemplateId = row.rewMailTemplateId;
    RewMailDelaySecs = row.rewMailDelaySecs;
    PointMapId = row.pointMapId;
    PointX = row.pointX;
    PointY = row.pointY;
    PointOpt = row.pointOpt;

    for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        DetailsEmote[i] = row.detailsEmote[i];
    }

    for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        DetailsEmoteDelay[i] = row.detailsEmoteDelay[i];
    }

    IncompleteEmote = row.incompleteEmote;
    CompleteEmote = row.completeEmote;

    for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        OfferRewardEmote[i] = row.offerRewardEmote[i];
    }

    for (int i = 0; i < QUEST_EMOTE_COUNT; ++i)
    {
        OfferRewardEmoteDelay[i] = row.offerRewardEmoteDelay[i];
    }

    SoundAcceptId = row.soundAccept;
    SoundTurnInId = row.soundTurnIn;

    QuestStartScript = row.startScript;
    QuestCompleteScript = row.completeScript;

    m_isActive = true;

    m_reqitemscount = 0;
    m_reqCreatureOrGOcount = 0;
    m_rewitemscount = 0;
    m_rewchoiceitemscount = 0;
    m_reqCurrencyCount = 0;

    for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; ++i)
    {
        if (ReqItemId[i])
        {
            ++m_reqitemscount;
        }
    }

    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
    {
        if (ReqCreatureOrGOId[i])
        {
            ++m_reqCreatureOrGOcount;
        }
    }

    for (int i = 0; i < QUEST_REWARDS_COUNT; ++i)
    {
        if (RewItemId[i])
        {
            ++m_rewitemscount;
        }
    }

    for (int i = 0; i < QUEST_REWARD_CHOICES_COUNT; ++i)
    {
        if (RewChoiceItemId[i])
        {
            ++m_rewchoiceitemscount;
        }
    }

    for (int i = 0; i < QUEST_REQUIRED_CURRENCY_COUNT; ++i)
    {
        if (ReqCurrencyId[i])
        {
            ++m_reqCurrencyCount;
        }
    }
}

/**
 * @brief Calculates the quest experience reward for a specific player.
 *
 * @param pPlayer The player receiving the quest reward.
 * @return uint32 The adjusted quest experience reward.
 */
uint32 Quest::XPValue(uint32 level) const
{
    if (level)
    {
        uint32 realXP = 0;
        uint32 xpMultiplier = 0;
        int32 baseLevel = 0;
        int32 playerLevel = int32(level);

        // formula can possibly be organized better, using less if's and simplify some.

        if (QuestLevel != -1)
        {
            baseLevel = QuestLevel;
        }

        if (((baseLevel - playerLevel) + 10) * 2 > 10)
        {
            baseLevel = playerLevel;

            if (QuestLevel != -1)
            {
                baseLevel = QuestLevel;
            }

            if (((baseLevel - playerLevel) + 10) * 2 <= 10)
            {
                if (QuestLevel == -1)
                {
                    baseLevel = playerLevel;
                }

                xpMultiplier = 2 * (baseLevel - playerLevel) + 20;
            }
            else
            {
                xpMultiplier = 10;
            }
        }
        else
        {
            baseLevel = playerLevel;

            if (QuestLevel != -1)
            {
                baseLevel = QuestLevel;
            }

            if (((baseLevel - playerLevel) + 10) * 2 >= 1)
            {
                baseLevel = playerLevel;

                if (QuestLevel != -1)
                {
                    baseLevel = QuestLevel;
                }

                if (((baseLevel - playerLevel) + 10) * 2 <= 10)
                {
                    if (QuestLevel == -1)
                    {
                        baseLevel = playerLevel;
                    }

                    xpMultiplier = 2 * (baseLevel - playerLevel) + 20;
                }
                else
                {
                    xpMultiplier = 10;
                }
            }
            else
            {
                xpMultiplier = 1;
            }
        }

        // not possible to reward XP when baseLevel does not exist in dbc
        if (const QuestXPLevel* pXPData = sQuestXPLevelStore.LookupEntry(baseLevel))
        {
            uint32 rawXP = xpMultiplier * pXPData->Difficulty[RewXPId] / 10;

            // round values
            if (rawXP > 1000)
            {
                realXP = ((rawXP + 25) / 50 * 50);
            }
            else if (rawXP > 500)
            {
                realXP = ((rawXP + 12) / 25 * 25);
            }
            else if (rawXP > 100)
            {
                realXP = ((rawXP + 5) / 10 * 10);
            }
            else
            {
                realXP = ((rawXP + 2) / 5 * 5);
            }
        }

        return realXP;
    }

    return 0;
}

/**
 * @brief Returns the quest reward or required money after server rate adjustments.
 *
 * @return int32 The adjusted reward or required money value.
 */
int32  Quest::GetRewOrReqMoney() const
{
    if (RewOrReqMoney <= 0)
    {
        return RewOrReqMoney;
    }

    return int32(RewOrReqMoney * sGameConfig.getConfig(CONFIG_FLOAT_RATE_DROP_MONEY));
}

/**
 * @brief Checks whether the quest may be completed while the player is in a raid.
 *
 * @return true if raid completion is allowed; otherwise false.
 */
bool Quest::IsAllowedInRaid() const
{
    if (Type == QUEST_TYPE_RAID || Type == QUEST_TYPE_RAID_10 || Type == QUEST_TYPE_RAID_25)
    {
        return true;
    }

    return sGameConfig.getConfig(CONFIG_BOOL_QUEST_IGNORE_RAID);
}

uint32 Quest::CalculateRewardHonor(uint32 level) const
{
    if (level > GT_MAX_LEVEL)
    {
        level = GT_MAX_LEVEL;
    }

    uint32 honor = 0;

    if (GetRewHonorAddition() > 0 || GetRewHonorMultiplier() > 0.0f)
    {
        // values stored from 0.. for 1...
        /* not exist in 4.x
        TeamContributionPoints const* tc = sTeamContributionPoints.LookupEntry(level-1);
        if (!tc)
        {
            return 0;
        }
        */
        uint32 i_honor = uint32(/*tc->Value*/1.0f * GetRewHonorMultiplier() * 0.1f);
        honor = i_honor + GetRewHonorAddition();
    }

    return honor;
}

