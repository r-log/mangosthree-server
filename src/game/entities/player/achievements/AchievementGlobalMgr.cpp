/*
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
 * @file AchievementGlobalMgr.cpp
 * @brief AchievementGlobalMgr's loaders (criteria, references, requirements, rewards, completed achievements) and
 * accessors.
 */

#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <string>
#include "AchievementMgr.h"
#include "Log.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "ProgressBar.h"
#include "persistence/characters/achievements/AchievementRepository.h"
#include "persistence/world/achievements/AchievementDataRepository.h"

static_assert(int(ACHIEVEMENT_REWARD_LOCALE_ROW_LOCALES) == int(MAX_LOCALE) - 1, "an achievement reward locale row holds its texts for every locale but the default one");

AchievementCriteriaEntryList const& AchievementGlobalMgr::GetAchievementCriteriaByType(AchievementCriteriaTypes type)
{
    return m_AchievementCriteriasByType[type];
}

AchievementCriteriaEntryList const* AchievementGlobalMgr::GetAchievementCriteriaByAchievement(uint32 id)
{
    AchievementCriteriaListByAchievement::const_iterator itr = m_AchievementCriteriaListByAchievement.find(id);
    return itr != m_AchievementCriteriaListByAchievement.end() ? &itr->second : NULL;
}

AchievementEntryList const* AchievementGlobalMgr::GetAchievementByReferencedId(uint32 id) const
{
    AchievementListByReferencedId::const_iterator itr = m_AchievementListByReferencedId.find(id);
    return itr != m_AchievementListByReferencedId.end() ? &itr->second : NULL;
}

AchievementReward const* AchievementGlobalMgr::GetAchievementReward(AchievementEntry const* achievement, uint8 gender) const
{
    AchievementRewardsMapBounds bounds = m_achievementRewards.equal_range(achievement->ID);
    for (AchievementRewardsMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
        if (iter->second.gender == GENDER_NONE || uint8(iter->second.gender) == gender)
        {
            return &iter->second;
        }

    return NULL;
}

AchievementRewardLocale const* AchievementGlobalMgr::GetAchievementRewardLocale(AchievementEntry const* achievement, uint8 gender) const
{
    AchievementRewardLocalesMapBounds bounds = m_achievementRewardLocales.equal_range(achievement->ID);
    for (AchievementRewardLocalesMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
        if (iter->second.gender == GENDER_NONE || uint8(iter->second.gender) == gender)
        {
            return &iter->second;
        }

    return NULL;
}

AchievementCriteriaRequirementSet const* AchievementGlobalMgr::GetCriteriaRequirementSet(AchievementCriteriaEntry const* achievementCriteria)
{
    AchievementCriteriaRequirementMap::const_iterator iter = m_criteriaRequirementMap.find(achievementCriteria->ID);
    return iter != m_criteriaRequirementMap.end() ? &iter->second : NULL;
}

bool AchievementGlobalMgr::IsRealmCompleted(AchievementEntry const* achievement) const
{
    return m_allCompletedAchievements.find(achievement->ID) != m_allCompletedAchievements.end();
}

void AchievementGlobalMgr::SetRealmCompleted(AchievementEntry const* achievement)
{
    m_allCompletedAchievements.insert(achievement->ID);
}

void AchievementGlobalMgr::LoadAchievementCriteriaList()
{
    if (sAchievementCriteriaStore.GetNumRows() == 0)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 achievement criteria.");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(sAchievementCriteriaStore.GetNumRows());
    for (uint32 entryId = 0; entryId < sAchievementCriteriaStore.GetNumRows(); ++entryId)
    {
        bar.step();

        AchievementCriteriaEntry const* criteria = sAchievementCriteriaStore.LookupEntry(entryId);
        if (!criteria)
        {
            continue;
        }

        MANGOS_ASSERT(criteria->requiredType < ACHIEVEMENT_CRITERIA_TYPE_TOTAL && "Not updated ACHIEVEMENT_CRITERIA_TYPE_TOTAL?");

        // check if referredAchievement exists!
        AchievementEntry const* achiev = sAchievementStore.LookupEntry(criteria->referredAchievement);
        if (!achiev)
        {
            sLog.outDetail("Removed achievement-criteria %u, because referred achievement does not exist", entryId);
            sAchievementCriteriaStore.EraseEntry(entryId);
            continue;
        }

        m_AchievementCriteriasByType[criteria->requiredType].push_back(criteria);
        m_AchievementCriteriaListByAchievement[criteria->referredAchievement].push_back(criteria);
        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u achievement criteria.", count);
}

void AchievementGlobalMgr::LoadAchievementReferenceList()
{
    if (sAchievementStore.GetNumRows() == 0)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 achievement references.");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(sAchievementStore.GetNumRows());
    for (uint32 entryId = 0; entryId < sAchievementStore.GetNumRows(); ++entryId)
    {
        bar.step();

        AchievementEntry const* achievement = sAchievementStore.LookupEntry(entryId);
        if (!achievement || !achievement->Shares_criteria)
        {
            continue;
        }

        // Check refAchievement exists
        AchievementEntry const* refAchiev = sAchievementStore.LookupEntry(achievement->Shares_criteria);
        if (!refAchiev)
        {
            sLog.outDetail("Removed achieviement %u, because referred achievement does not exist", entryId);
            sAchievementStore.EraseEntry(entryId);
            continue;
        }

        m_AchievementListByReferencedId[achievement->Shares_criteria].push_back(achievement);
        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u achievement references.", count);
}

void AchievementGlobalMgr::LoadAchievementCriteriaRequirements()
{
    m_criteriaRequirementMap.clear();                       // need for reload case

    std::vector<AchievementCriteriaRequirementRow> rows = AchievementDataRepository::QueryCriteriaRequirements();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 additional achievement criteria data. DB table `achievement_criteria_requirement` is empty.");
        return;
    }

    uint32 count = 0;
    uint32 disabled_count = 0;
    BarGoLink bar(rows.size());
    for (AchievementCriteriaRequirementRow const& row : rows)
    {
        bar.step();
        uint32 criteria_id = row.criteriaId;

        AchievementCriteriaEntry const* criteria = sAchievementCriteriaStore.LookupEntry(criteria_id);

        if (!criteria)
        {
            sLog.outErrorDb("Table `achievement_criteria_requirement`.`criteria_id` %u does not exist, ignoring.", criteria_id);
            continue;
        }

        AchievementCriteriaRequirement data(row.type, row.value1, row.value2);

        if (!data.IsValid(criteria))
        {
            continue;
        }

        // this will allocate empty data set storage
        AchievementCriteriaRequirementSet& dataSet = m_criteriaRequirementMap[criteria_id];
        dataSet.SetCriteriaId(criteria_id);

        // counting disable criteria requirements
        if (data.requirementType == ACHIEVEMENT_CRITERIA_REQUIRE_DISABLED)
        {
            ++disabled_count;
        }

        // add real data only for not NONE requirements
        if (data.requirementType != ACHIEVEMENT_CRITERIA_REQUIRE_NONE)
        {
            dataSet.Add(data);
        }

        // counting requirements
        ++count;
    }

    // post loading checks
    for (uint32 entryId = 0; entryId < sAchievementCriteriaStore.GetNumRows(); ++entryId)
    {
        AchievementCriteriaEntry const* criteria = sAchievementCriteriaStore.LookupEntry(entryId);
        if (!criteria)
        {
            continue;
        }

        switch (criteria->requiredType)
        {
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_BG:
                if (!criteria->win_bg.additionalRequirement1_type && !criteria->win_bg.additionalRequirement2_type)
                {
                    continue;
                }
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_KILL_CREATURE:
                break;                                      // any cases
            case ACHIEVEMENT_CRITERIA_TYPE_COMPLETE_QUEST:
            {
                AchievementEntry const* achievement = sAchievementStore.LookupEntry(criteria->referredAchievement);
                // Checked in LoadAchievementCriteriaList

                // exist many achievements with this criteria, use at this moment hardcoded check to skil simple case
                switch (achievement->ID)
                {
                    case 31:
                        // case 1275: // these timed achievements are "started" on Quest Accept, and simple ended on quest-complete
                        // case 1276:
                        // case 1277:
                    case 1282:
                    case 1789:
                        break;
                    default:
                        continue;
                }
            }
            case ACHIEVEMENT_CRITERIA_TYPE_FALL_WITHOUT_DYING:
                break;                                      // any cases
            case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL:      // any cases
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_RATED_ARENA: // need skip generic cases
                if (criteria->win_rated_arena.flag != ACHIEVEMENT_CRITERIA_CONDITION_NO_LOOSE)
                {
                    continue;
                }
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_EQUIP_EPIC_ITEM: // any cases
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE:        // need skip generic cases
                if (criteria->do_emote.count == 0)
                {
                    continue;
                }
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_SPECIAL_PVP_KILL:// any cases
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_WIN_DUEL:        // skip statistics
                if (criteria->win_duel.duelCount == 0)
                {
                    continue;
                }
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_CAST_SPELL2:     // any cases
                break;
            case ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE:       // need skip generic cases
                if (criteria->loot_type.lootTypeCount != 1)
                {
                    continue;
                }
                break;
            default:                                        // type not use DB data, ignore
                continue;
        }

        if (!GetCriteriaRequirementSet(criteria))
            sLog.outErrorDb("Table `achievement_criteria_requirement` is missing expected data for `criteria_id` %u (type: %u) for achievement %u.", criteria->ID, criteria->requiredType, criteria->referredAchievement);
    }

    sLog.outString();
    sLog.outString(">> Loaded %u additional achievement criteria data (%u disabled).", count, disabled_count);
}

void AchievementGlobalMgr::LoadCompletedAchievements()
{
    std::vector<CompletedAchievementRow> rows = AchievementRepository::QueryCompletedAchievements();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 realm completed achievements . DB table `character_achievement` is empty.");
        return;
    }

    BarGoLink bar(rows.size());
    for (CompletedAchievementRow const& row : rows)
    {
        bar.step();

        uint32 achievement_id = row.achievement;
        if (!sAchievementStore.LookupEntry(achievement_id))
        {
            // the rows of a nonexistent achievement are kept and logged
            sLog.outError("Nonexistent achievement %u data kept in table `character_achievement`.", achievement_id);
            continue;
        }

        m_allCompletedAchievements.insert(achievement_id);
    }

    sLog.outString();
    sLog.outString(">> Loaded %zu realm completed achievements.", m_allCompletedAchievements.size());
}

void AchievementGlobalMgr::ReportOrphanedCriteriaProgress()
{
    std::vector<AchievementCriteriaProgressRow> rows = AchievementRepository::QueryProgressCriteria();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Found 0 orphaned criteria data. DB table `character_achievement_progress` is empty.");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(rows.size());
    for (AchievementCriteriaProgressRow const& row : rows)
    {
        bar.step();

        uint32 criteria_id = row.criteria;
        if (!sAchievementCriteriaStore.LookupEntry(criteria_id))
        {
            // the rows of a nonexistent criteria are kept and logged
            sLog.outError("Nonexistent achievement criteria %u data kept in table `character_achievement_progress`.", criteria_id);
            ++count;
        }
    }

    sLog.outString();
    sLog.outString(">> Kept %u orphaned criteria data in table `character_achievement_progress`.", count);
}

void AchievementGlobalMgr::LoadRewards()
{
    m_achievementRewards.clear();                           // need for reload case

    std::vector<AchievementRewardRow> rows = AchievementDataRepository::QueryRewards();

    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 achievement rewards. DB table `achievement_reward` is empty.");
        return;
    }

    uint32 count = 0;
    BarGoLink bar(rows.size());

    for (AchievementRewardRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;
        if (!sAchievementStore.LookupEntry(entry))
        {
            sLog.outErrorDb("Table `achievement_reward` has wrong achievement (Entry: %u), ignore", entry);
            continue;
        }

        AchievementReward reward;
        reward.gender     = Gender(row.gender);
        reward.titleId[0] = row.titleA;
        reward.titleId[1] = row.titleH;
        reward.itemId     = row.item;
        reward.sender     = row.sender;
        reward.subject    = row.subject;
        reward.text       = row.text;

        if (reward.gender >= MAX_GENDER)
        {
            sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has wrong gender %u.", entry, reward.gender);
        }

        // GENDER_NONE must be single (so or already in and none must be attempt added new data or just adding and none in)
        // other duplicate cases prevented by DB primary key
        bool dup = false;
        AchievementRewardsMapBounds bounds = m_achievementRewards.equal_range(entry);
        for (AchievementRewardsMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
        {
            if (iter->second.gender == GENDER_NONE || reward.gender == GENDER_NONE)
            {
                dup = true;
                sLog.outErrorDb("Table `achievement_reward` must have single GENDER_NONE (%u) case (Entry: %u), ignore duplicate case", GENDER_NONE, entry);
                break;
            }
        }
        if (dup)
        {
            continue;
        }

        if ((reward.titleId[0] == 0) != (reward.titleId[1] == 0))
            sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has title (A: %u H: %u) only for one from teams.", entry, reward.titleId[0], reward.titleId[1]);

        // must be title or mail at least
        if (!reward.titleId[0] && !reward.titleId[1] && !reward.sender)
        {
            sLog.outErrorDb("Table `achievement_reward` (Entry: %u) not have title or item reward data, ignore.", entry);
            continue;
        }

        if (reward.titleId[0])
        {
            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(reward.titleId[0]);
            if (!titleEntry)
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has invalid title id (%u) in `title_A`, set to 0", entry, reward.titleId[0]);
                reward.titleId[0] = 0;
            }
        }

        if (reward.titleId[1])
        {
            CharTitlesEntry const* titleEntry = sCharTitlesStore.LookupEntry(reward.titleId[1]);
            if (!titleEntry)
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has invalid title id (%u) in `title_A`, set to 0", entry, reward.titleId[1]);
                reward.titleId[1] = 0;
            }
        }

        // check mail data before item for report including wrong item case
        if (reward.sender)
        {
            if (!ObjectMgr::GetCreatureTemplate(reward.sender))
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has invalid creature entry %u as sender, mail reward skipped.", entry, reward.sender);
                reward.sender = 0;
            }
        }
        else
        {
            if (reward.itemId)
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) not have sender data but have item reward, item will not rewarded", entry);
            }

            if (!reward.subject.empty())
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) not have sender data but have mail subject.", entry);
            }

            if (!reward.text.empty())
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) not have sender data but have mail text.", entry);
            }
        }

        if (reward.itemId)
        {
            if (!ObjectMgr::GetItemPrototype(reward.itemId))
            {
                sLog.outErrorDb("Table `achievement_reward` (Entry: %u) has invalid item id %u, reward mail will be without item.", entry, reward.itemId);
                reward.itemId = 0;
            }
        }

        m_achievementRewards.insert(AchievementRewardsMap::value_type(entry, reward));
        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u achievement rewards", count);
}

void AchievementGlobalMgr::LoadRewardLocales()
{
    m_achievementRewardLocales.clear();                     // need for reload case

    std::vector<AchievementRewardLocaleRow> rows = AchievementDataRepository::QueryRewardLocales();
    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 achievement reward locale strings. DB table `locales_achievement_reward` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (AchievementRewardLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (m_achievementRewards.find(entry) == m_achievementRewards.end())
        {
            sLog.outErrorDb("Table `locales_achievement_reward` (Entry: %u) has locale strings for nonexistent achievement reward .", entry);
            continue;
        }

        AchievementRewardLocale data;

        data.gender = Gender(row.gender);

        if (data.gender >= MAX_GENDER)
        {
            sLog.outErrorDb("Table `locales_achievement_reward` (Entry: %u) has wrong gender %u.", entry, data.gender);
        }

        // GENDER_NONE must be single (so or already in and none must be attempt added new data or just adding and none in)
        // other duplicate cases prevented by DB primary key
        bool dup = false;
        AchievementRewardLocalesMapBounds bounds = m_achievementRewardLocales.equal_range(entry);
        for (AchievementRewardLocalesMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
        {
            if (iter->second.gender == GENDER_NONE || data.gender == GENDER_NONE)
            {
                dup = true;
                sLog.outErrorDb("Table `locales_achievement_reward` must have single GENDER_NONE (%u) case (Entry: %u), ignore duplicate case", GENDER_NONE, entry);
                break;
            }
        }
        if (dup)
        {
            continue;
        }

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.subject[i - 1];
            if (!str.empty())
            {
                int idx = sObjectMgr.GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if (data.subject.size() <= size_t(idx))
                    {
                        data.subject.resize(idx + 1);
                    }

                    data.subject[idx] = str;
                }
            }
            str = row.text[i - 1];
            if (!str.empty())
            {
                int idx = sObjectMgr.GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if (data.text.size() <= size_t(idx))
                    {
                        data.text.resize(idx + 1);
                    }

                    data.text[idx] = str;
                }
            }
        }

        m_achievementRewardLocales.insert(AchievementRewardLocalesMap::value_type(entry, data));
    }

    sLog.outString();
    sLog.outString(">> Loaded %zu achievement reward locale strings", m_achievementRewardLocales.size());
}
