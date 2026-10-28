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
 * @file ObjectMgrQuests.cpp
 * @brief ObjectMgr's quest template and exclusive-group lookups and the quest and quest locale loaders.
 */

#include "Common/Locales.h"
#include <string>
#include <map>
#include "ObjectMgr.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "Log.h"
#include "ObjectGuid.h"
#include "spells/data/SpellMgr.h"
#include "data/GameConfig.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "DisableMgr.h"

#include <limits>
#include "persistence/world/quests/QuestDataRepository.h"

static_assert(int(QUEST_LOCALE_ROW_LOCALES) == int(MAX_LOCALE) - 1, "a quest locale row holds its texts for every locale but the default one");
static_assert(int(QUEST_LOCALE_ROW_OBJECTIVES) == int(QUEST_OBJECTIVES_COUNT), "a quest locale row holds every objective text");

/**
 * @brief Loads quest templates and validates quest-related database references.
 */
void ObjectMgr::LoadQuests()
{
    // For reload case
    for (QuestMap::const_iterator itr = mQuestTemplates.begin(); itr != mQuestTemplates.end(); ++itr)
    {
        delete itr->second;
    }

    mQuestTemplates.clear();

    m_ExclusiveQuestGroups.clear();

    std::vector<QuestTemplateRow> rows = QuestDataRepository::QueryQuests();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Loaded 0 quests definitions");
        sLog.outErrorDb("`quest_template` table is empty!");
        sLog.outString();
        return;
    }

    // create multimap previous quest for each existing quest
    // some quests can have many previous maps set by NextQuestId in previous quest
    // for example set of race quests can lead to single not race specific quest
    BarGoLink bar(rows.size());
    for (QuestTemplateRow const& row : rows)
    {
        bar.step();

        Quest* newQuest = new Quest(row);
        mQuestTemplates[newQuest->GetQuestId()] = newQuest;
    }

    // Post processing

    std::map<uint32, uint32> usedMailTemplates;

    for (QuestMap::iterator iter = mQuestTemplates.begin(); iter != mQuestTemplates.end(); ++iter)
    {
        // skip post-loading checks for disabled quests
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, iter->first))
        {
            continue;
        }

        Quest* qinfo = iter->second;

        // additional quest integrity checks (GO, creature_template and item_template must be loaded already)

        if (qinfo->GetQuestMethod() >= 3)
        {
            sLog.outErrorDb("Quest %u has `Method` = %u, expected values are 0, 1 or 2.", qinfo->GetQuestId(), qinfo->GetQuestMethod());
        }

        if (qinfo->m_SpecialFlags > QUEST_SPECIAL_FLAG_DB_ALLOWED)
        {
            sLog.outErrorDb("Quest %u has `SpecialFlags` = %u, above max flags not allowed for database.", qinfo->GetQuestId(), qinfo->m_SpecialFlags);
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_DAILY) && qinfo->HasQuestFlag(QUEST_FLAGS_WEEKLY))
        {
            sLog.outErrorDb("Weekly Quest %u is marked as daily quest in `QuestFlags`, removed daily flag.", qinfo->GetQuestId());
            qinfo->m_QuestFlags &= ~QUEST_FLAGS_DAILY;
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_DAILY))
        {
            if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE))
            {
                sLog.outErrorDb("Daily Quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE);
            }
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_WEEKLY))
        {
            if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE))
            {
                sLog.outErrorDb("Weekly Quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE);
            }
        }

        if (qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_MONTHLY))
        {
            if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE))
            {
                sLog.outErrorDb("Monthly quest %u not marked as repeatable in `SpecialFlags`, added.", qinfo->GetQuestId());
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_REPEATABLE);
            }
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
        {
            // at auto-reward can be rewarded only RewChoiceItemId[0]
            for (int j = 1; j < QUEST_REWARD_CHOICES_COUNT; ++j)
            {
                if (uint32 id = qinfo->RewChoiceItemId[j])
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but item from `RewChoiceItemId%d` can't be rewarded with quest flag QUEST_FLAGS_AUTO_REWARDED.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest ignore this data
                }
            }
        }

        // client quest log visual (area case)
        if (qinfo->ZoneOrSort > 0)
        {
            if (!GetAreaEntryByAreaID(qinfo->ZoneOrSort))
            {
                sLog.outErrorDb("Quest %u has `ZoneOrSort` = %u (zone case) but zone with this id does not exist.",
                                qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have problems at client
            }
        }
        // client quest log visual (sort case)
        if (qinfo->ZoneOrSort < 0)
        {
            QuestSortEntry const* qSort = sQuestSortStore.LookupEntry(-int32(qinfo->ZoneOrSort));
            if (!qSort)
            {
                sLog.outErrorDb("Quest %u has `ZoneOrSort` = %i (sort case) but quest sort with this id does not exist.",
                                qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have problems at client (note some may be 0, we must allow this so no check)
            }
        }

        // RequiredClasses, can be 0/CLASSMASK_ALL_PLAYABLE to allow any class
        if (qinfo->RequiredClasses)
        {
            if (!(qinfo->RequiredClasses & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Quest %u does not contain any playable classes in `RequiredClasses` (%u), value set to 0 (all classes).", qinfo->GetQuestId(), qinfo->RequiredClasses);
                qinfo->RequiredClasses = 0;
            }
        }

        // RequiredRaces, can be 0/RACEMASK_ALL_PLAYABLE to allow any race
        if (qinfo->RequiredRaces)
        {
            if (!(qinfo->RequiredRaces & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Quest %u does not contain any playable races in `RequiredRaces` (%u), value set to 0 (all races).", qinfo->GetQuestId(), qinfo->RequiredRaces);
                qinfo->RequiredRaces = 0;
            }
        }

        // RequiredSkill, can be 0
        if (qinfo->RequiredSkill)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->RequiredSkill))
            {
                sLog.outErrorDb("Quest %u has `RequiredSkill` = %u but this skill does not exist",
                                qinfo->GetQuestId(), qinfo->RequiredSkill);
            }
        }

        if (qinfo->RequiredSkillValue)
        {
            if (qinfo->RequiredSkillValue > sGameConfig.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Quest %u has `RequiredSkillValue` = %u but max possible skill is %u, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->RequiredSkillValue, sGameConfig.GetConfigMaxSkillValue());
                // no changes, quest can't be done for this requirement
            }
        }
        // else Skill quests can have 0 skill level, this is ok

        if (qinfo->RepObjectiveFaction && !sFactionStore.LookupEntry(qinfo->RepObjectiveFaction))
        {
            sLog.outErrorDb("Quest %u has `RepObjectiveFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RepObjectiveFaction, qinfo->RepObjectiveFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepFaction && !sFactionStore.LookupEntry(qinfo->RequiredMinRepFaction))
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepFaction, qinfo->RequiredMinRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMaxRepFaction && !sFactionStore.LookupEntry(qinfo->RequiredMaxRepFaction))
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepFaction, qinfo->RequiredMaxRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue && qinfo->RequiredMinRepValue > ReputationMgr::Reputation_Cap)
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepValue` = %d but max reputation is %u, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepValue, ReputationMgr::Reputation_Cap);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue && qinfo->RequiredMaxRepValue && qinfo->RequiredMaxRepValue <= qinfo->RequiredMinRepValue)
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepValue` = %d and `RequiredMinRepValue` = %d, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepValue, qinfo->RequiredMinRepValue);
            // no changes, quest can't be done for this requirement
        }

        if (!qinfo->RepObjectiveFaction && qinfo->RepObjectiveValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RepObjectiveValue` = %d but `RepObjectiveFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RepObjectiveValue);
            // warning
        }

        if (!qinfo->RequiredMinRepFaction && qinfo->RequiredMinRepValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepValue` = %d but `RequiredMinRepFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepValue);
            // warning
        }

        if (!qinfo->RequiredMaxRepFaction && qinfo->RequiredMaxRepValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepValue` = %d but `RequiredMaxRepFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepValue);
            // warning
        }

        if (qinfo->CharTitleId && !sCharTitlesStore.LookupEntry(qinfo->CharTitleId))
        {
            sLog.outErrorDb("Quest %u has `CharTitleId` = %u but CharTitle Id %u does not exist, quest can't be rewarded with title.",
                            qinfo->GetQuestId(), qinfo->GetCharTitleId(), qinfo->GetCharTitleId());
            qinfo->CharTitleId = 0;
            // quest can't reward this title
        }

        if (qinfo->SrcItemId)
        {
            if (!ObjectMgr::GetItemPrototype(qinfo->SrcItemId))
            {
                sLog.outErrorDb("Quest %u has `SrcItemId` = %u but item with entry %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcItemId, qinfo->SrcItemId);
                qinfo->SrcItemId = 0;                       // quest can't be done for this requirement
            }
            else if (qinfo->SrcItemCount == 0)
            {
                sLog.outErrorDb("Quest %u has `SrcItemId` = %u but `SrcItemCount` = 0, set to 1 but need fix in DB.",
                                qinfo->GetQuestId(), qinfo->SrcItemId);
                qinfo->SrcItemCount = 1;                    // update to 1 for allow quest work for backward compatibility with DB
            }
        }
        else if (qinfo->SrcItemCount > 0)
        {
            sLog.outErrorDb("Quest %u has `SrcItemId` = 0 but `SrcItemCount` = %u, useless value.",
                            qinfo->GetQuestId(), qinfo->SrcItemCount);
            qinfo->SrcItemCount = 0;                        // no quest work changes in fact
        }

        if (qinfo->SrcSpell)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->SrcSpell);
            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `SrcSpell` = %u but spell %u doesn't exist, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0;                        // quest can't be done for this requirement
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `SrcSpell` = %u but spell %u is broken, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0;                        // quest can't be done for this requirement
            }
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqItemId[j])
            {
                if (qinfo->ReqItemCount[j] == 0)
                {
                    sLog.outErrorDb("Quest %u has `ReqItemId%d` = %u but `ReqItemCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done for this requirement
                }

                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER);

                if (!ObjectMgr::GetItemPrototype(id))
                {
                    sLog.outErrorDb("Quest %u has `ReqItemId%d` = %u but item with entry %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->ReqItemCount[j] = 0;             // prevent incorrect work of quest
                }
            }
            else if (qinfo->ReqItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `ReqItemId%d` = 0 but `ReqItemCount%d` = %u, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqItemCount[j]);
                qinfo->ReqItemCount[j] = 0;                 // prevent incorrect work of quest
            }
        }

        for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSourceId[j])
            {
                if (!ObjectMgr::GetItemPrototype(id))
                {
                    sLog.outErrorDb("Quest %u has `ReqSourceId%d` = %u but item with entry %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    // no changes, quest can't be done for this requirement
                }
            }
            else
            {
                if (qinfo->ReqSourceCount[j] > 0)
                {
                    sLog.outErrorDb("Quest %u has `ReqSourceId%d` = 0 but `ReqSourceCount%d` = %u.",
                                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqSourceCount[j]);
                    // no changes, quest ignore this data
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSpell[j])
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
                if (!spellInfo)
                {
                    sLog.outErrorDb("Quest %u has `ReqSpellCast%d` = %u but spell %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    continue;
                }

                if (!qinfo->ReqCreatureOrGOId[j])
                {
                    bool found = false;
                    for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
                    {
                        SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(k));
                        if (!spellEffect)
                        {
                            continue;
                        }

                        if ((spellEffect->Effect == SPELL_EFFECT_QUEST_COMPLETE && uint32(spellEffect->EffectMiscValue_0) == qinfo->QuestId) ||
                            spellEffect->Effect == SPELL_EFFECT_SEND_EVENT)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (found)
                    {
                        if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
                        {
                            sLog.outErrorDb("Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE or SPELL_EFFECT_SEND_EVENT for quest %u and ReqCreatureOrGOId%d = 0, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Quest flags or ReqCreatureOrGOId%d must be fixed, quest modified to enable objective.", spellInfo->ID, qinfo->QuestId, j + 1, j + 1);

                            // this will prevent quest completing without objective
                            const_cast<Quest*>(qinfo)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
                        }
                    }
                    else
                    {
                        sLog.outErrorDb("Quest %u has `ReqSpellCast%d` = %u and ReqCreatureOrGOId%d = 0 but spell %u does not have SPELL_EFFECT_QUEST_COMPLETE or SPELL_EFFECT_SEND_EVENT effect for this quest, quest can't be done.",
                                        qinfo->GetQuestId(), j + 1, id, j + 1, id);
                        // no changes, quest can't be done for this requirement
                    }
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 id = qinfo->ReqCreatureOrGOId[j];
            if (id < 0 && !ObjectMgr::GetGameObjectInfo(-id))
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %i but gameobject %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, id, uint32(-id));
                qinfo->ReqCreatureOrGOId[j] = 0;            // quest can't be done for this requirement
            }

            if (id > 0 && !ObjectMgr::GetCreatureTemplate(id))
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %i but creature with entry %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, id, uint32(id));
                qinfo->ReqCreatureOrGOId[j] = 0;            // quest can't be done for this requirement
            }

            if (id)
            {
                // In fact SpeakTo and Kill are quite same: either you can speak to mob:SpeakTo or you can't:Kill/Cast

                qinfo->SetSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO));

                if (!qinfo->ReqCreatureOrGOCount[j])
                {
                    sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %u but `ReqCreatureOrGOCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can be incorrectly done, but we already report this
                }
            }
            else if (qinfo->ReqCreatureOrGOCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = 0 but `ReqCreatureOrGOCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqCreatureOrGOCount[j]);
                // no changes, quest ignore this data
            }
        }

        bool choice_found = false;
        for (int j = QUEST_REWARD_CHOICES_COUNT - 1; j >= 0; --j)
        {
            if (uint32 id = qinfo->RewChoiceItemId[j])
            {
                if (!ObjectMgr::GetItemPrototype(id))
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewChoiceItemId[j] = 0;          // no changes, quest will not reward this
                }
                else
                {
                    choice_found = true;
                }

                if (!qinfo->RewChoiceItemCount[j])
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but `RewChoiceItemCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done
                }
            }
            else if (choice_found)                          // client crash if have gap in item reward choices
            {
                sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = 0 but `RewChoiceItemId%d` = %u, client can crash at like data.",
                                qinfo->GetQuestId(), j + 1, j + 2, qinfo->RewChoiceItemId[j + 1]);
                // fill gap by clone later filled choice
                qinfo->RewChoiceItemId[j] = qinfo->RewChoiceItemId[j + 1];
                qinfo->RewChoiceItemCount[j] = qinfo->RewChoiceItemCount[j + 1];
            }
            else if (qinfo->RewChoiceItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = 0 but `RewChoiceItemCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewChoiceItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REWARDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->RewItemId[j])
            {
                if (!ObjectMgr::GetItemPrototype(id))
                {
                    sLog.outErrorDb("Quest %u has `RewItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewItemId[j] = 0;                // no changes, quest will not reward this item
                }

                if (!qinfo->RewItemCount[j])
                {
                    sLog.outErrorDb("Quest %u has `RewItemId%d` = %u but `RewItemCount%d` = 0, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes
                }
            }
            else if (qinfo->RewItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `RewItemId%d` = 0 but `RewItemCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REPUTATIONS_COUNT; ++j)
        {
            if (qinfo->RewRepFaction[j])
            {
                if (abs(qinfo->RewRepValueId[j]) > 9)
                {
                    sLog.outErrorDb("Quest %u has RewRepValueId%d = %i but value is not valid.", qinfo->GetQuestId(), j + 1, qinfo->RewRepValueId[j]);
                }

                if (!sFactionStore.LookupEntry(qinfo->RewRepFaction[j]))
                {
                    sLog.outErrorDb("Quest %u has `RewRepFaction%d` = %u but raw faction (faction.dbc) %u does not exist, quest will not reward reputation for this faction.",
                                    qinfo->GetQuestId(), j + 1, qinfo->RewRepFaction[j] , qinfo->RewRepFaction[j]);
                    qinfo->RewRepFaction[j] = 0;            // quest will not reward this
                }
            }
            else if (qinfo->RewRepValue[j] != 0)
            {
                sLog.outErrorDb("Quest %u has `RewRepFaction%d` = 0 but `RewRepValue%d` = %i.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewRepValue[j]);
                // no changes, quest ignore this data
            }
        }

        if (qinfo->RewSpell)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->RewSpell);

            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u does not exist, spell removed as display reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is broken, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
            else if (GetTalentSpellCost(qinfo->RewSpell))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is talent, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
        }

        if (qinfo->RewSpellCast)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->RewSpellCast);

            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `RewSpellCast` = %u but spell %u does not exist, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `RewSpellCast` = %u but spell %u is broken, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
            else if (GetTalentSpellCost(qinfo->RewSpellCast))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is talent, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
        }

        if (qinfo->RewMailTemplateId)
        {
            if (!sMailTemplateStore.LookupEntry(qinfo->RewMailTemplateId))
            {
                sLog.outErrorDb("Quest %u has `RewMailTemplateId` = %u but mail template  %u does not exist, quest will not have a mail reward.",
                                qinfo->GetQuestId(), qinfo->RewMailTemplateId, qinfo->RewMailTemplateId);
                qinfo->RewMailTemplateId = 0;               // no mail will send to player
                qinfo->RewMailDelaySecs = 0;                // no mail will send to player
            }
            else if (usedMailTemplates.find(qinfo->RewMailTemplateId) != usedMailTemplates.end())
            {
                std::map<uint32, uint32>::const_iterator used_mt_itr = usedMailTemplates.find(qinfo->RewMailTemplateId);
                sLog.outErrorDb("Quest %u has `RewMailTemplateId` = %u but mail template  %u already used for quest %u, quest will not have a mail reward.",
                                qinfo->GetQuestId(), qinfo->RewMailTemplateId, qinfo->RewMailTemplateId, used_mt_itr->second);
                qinfo->RewMailTemplateId = 0;               // no mail will send to player
                qinfo->RewMailDelaySecs = 0;                // no mail will send to player
            }
            else
            {
                usedMailTemplates[qinfo->RewMailTemplateId] = qinfo->GetQuestId();
            }
        }

        if (qinfo->NextQuestInChain)
        {
            QuestMap::iterator qNextItr = mQuestTemplates.find(qinfo->NextQuestInChain);
            if (qNextItr == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %u has `NextQuestInChain` = %u but quest %u does not exist, quest chain will not work.",
                                qinfo->GetQuestId(), qinfo->NextQuestInChain , qinfo->NextQuestInChain);
                qinfo->NextQuestInChain = 0;
            }
            else
            {
                qNextItr->second->prevChainQuests.push_back(qinfo->GetQuestId());
            }
        }

        // fill additional data stores
        if (qinfo->PrevQuestId)
        {
            if (mQuestTemplates.find(abs(qinfo->GetPrevQuestId())) == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %d has PrevQuestId %i, but no such quest", qinfo->GetQuestId(), qinfo->GetPrevQuestId());
            }
            else
            {
                qinfo->prevQuests.push_back(qinfo->PrevQuestId);
            }
        }

        if (qinfo->NextQuestId)
        {
            QuestMap::iterator qNextItr = mQuestTemplates.find(abs(qinfo->GetNextQuestId()));
            if (qNextItr == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %d has NextQuestId %i, but no such quest", qinfo->GetQuestId(), qinfo->GetNextQuestId());
            }
            else
            {
                int32 signedQuestId = qinfo->NextQuestId < 0 ? -int32(qinfo->GetQuestId()) : int32(qinfo->GetQuestId());
                qNextItr->second->prevQuests.push_back(signedQuestId);
            }
        }

        if (qinfo->RewSkill)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->RewSkill))
            {
                sLog.outErrorDb("Quest %u has `RewSkill` = %u but this skill does not exist",
                                qinfo->GetQuestId(), qinfo->RewSkill);
                qinfo->RewSkill = 0;
                qinfo->RewSkillValue = 0;
            }
        }

        if (qinfo->RewSkillValue)
        {
            if (qinfo->RewSkillValue > sGameConfig.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Quest %u has `RewSkillValue` = %u which is more than max possible skill value %u.",
                                qinfo->GetQuestId(), qinfo->RewSkillValue, sGameConfig.GetConfigMaxSkillValue());
            }
        }
        else
        {
            if (qinfo->RewSkill)
            {
                sLog.outErrorDb("Quest %u has `RewSkillValue` = %u, but `RewSkill` exists and is %u.",
                                    qinfo->GetQuestId(), qinfo->RewSkillValue, qinfo->RewSkill);
                qinfo->RewSkill = 0;
            }
        }

        if (qinfo->ReqSpellLearned)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->ReqSpellLearned);

            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `ReqSpellLearned` = %u but spell %u does not exist, quest will not have a spell requirement.",
                                qinfo->GetQuestId(), qinfo->ReqSpellLearned, qinfo->ReqSpellLearned);
                qinfo->ReqSpellLearned = 0;
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `ReqSpellLearned` = %u but spell %u is broken, quest will not have a spell requirement.",
                                qinfo->GetQuestId(), qinfo->ReqSpellLearned, qinfo->ReqSpellLearned);
                qinfo->ReqSpellLearned = 0;
            }
        }

        for (int j = 0; j < QUEST_REQUIRED_CURRENCY_COUNT; ++j)
        {
            if (qinfo->ReqCurrencyId[j])
            {
                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER);

                CurrencyTypesEntry const * currencyEntry = sCurrencyTypesStore.LookupEntry(qinfo->ReqCurrencyId[j]);
                if (!currencyEntry)
                {
                    sLog.outErrorDb("Quest %u has `ReqCurrencyId%d` = %u but currency with entry %u does not exist, quest can not be completed.",
                                    qinfo->GetQuestId(), j + 1, qinfo->ReqCurrencyId[j], qinfo->ReqCurrencyId[j]);
                    qinfo->ReqCurrencyId[j] = 0;
                    qinfo->ReqCurrencyCount[j] = 0;
                }
                else
                {
                    if (!qinfo->ReqCurrencyCount[j])
                    {
                        sLog.outErrorDb("Quest %u has `ReqCurrencyId%d` = %u but `ReqCurrencyCount%d` = %u.",
                                        qinfo->GetQuestId(), j + 1, qinfo->ReqCurrencyId[j], j + 1, qinfo->ReqCurrencyCount[j]);
                        qinfo->ReqCurrencyId[j] = 0;
                    }
                    else if (currencyEntry->MaxQty && qinfo->ReqCurrencyCount[j] > currencyEntry->MaxQty)
                    {
                        sLog.outErrorDb("Quest %u has `ReqCurrencyCount%d` = %u but currency %u has max count %u.",
                                        qinfo->GetQuestId(), j + 1, qinfo->ReqCurrencyCount[j], qinfo->ReqCurrencyId[j], currencyEntry->MaxQty);
                        qinfo->ReqCurrencyCount[j] = currencyEntry->MaxQty;
                    }
                }
            }
            else if (qinfo->ReqCurrencyCount[j])
            {
                if (!qinfo->ReqCurrencyId[j])
                {
                    sLog.outErrorDb("Quest %u has `ReqCurrencyId%d` = 0 but `ReqCurrencyCount%d` = %u.",
                                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqCurrencyCount[j]);
                    qinfo->ReqCurrencyCount[j] = 0;
                }
            }
        }

        for (int j = 0; j < QUEST_REWARD_CURRENCY_COUNT; ++j)
        {
            if (qinfo->RewCurrencyId[j])
            {
                CurrencyTypesEntry const * currencyEntry = sCurrencyTypesStore.LookupEntry(qinfo->RewCurrencyId[j]);
                if (!currencyEntry)
                {
                    sLog.outErrorDb("Quest %u has `RewCurrencyId%d` = %u but currency with entry %u does not exist, quest will not reward that currency.",
                                    qinfo->GetQuestId(), j + 1, qinfo->RewCurrencyId[j], qinfo->RewCurrencyId[j]);
                    qinfo->RewCurrencyId[j] = 0;
                    qinfo->RewCurrencyCount[j] = 0;
                }
                else if (!qinfo->RewCurrencyCount[j])
                {
                    sLog.outErrorDb("Quest %u has `RewCurrencyId%d` = %u but `RewCurrencyCount%d` = %u.",
                                    qinfo->GetQuestId(), j + 1, qinfo->RewCurrencyId[j], j + 1, qinfo->RewCurrencyCount[j]);
                    qinfo->RewCurrencyId[j] = 0;
                }
            }
            else if (qinfo->RewCurrencyCount[j])
            {
                if (!qinfo->RewCurrencyId[j])
                {
                    sLog.outErrorDb("Quest %u has `RewCurrencyId%d` = 0 but `RewCurrencyCount%d` = %u.",
                                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewCurrencyCount[j]);
                    qinfo->RewCurrencyCount[j] = 0;
                }
            }
        }

        if (qinfo->SoundAcceptId)
        {
            SoundEntriesEntry const * soundEntry = sSoundEntriesStore.LookupEntry(qinfo->SoundAcceptId);
            if (!soundEntry)
            {
                sLog.outErrorDb("Quest %u has `SoundAcceptId` = %u but sound with that entry does not exists.",
                   qinfo->GetQuestId(), qinfo->SoundAcceptId);
                qinfo->SoundAcceptId = 0;
            }
        }

        if (qinfo->SoundTurnInId)
        {
            SoundEntriesEntry const * soundEntry = sSoundEntriesStore.LookupEntry(qinfo->SoundTurnInId);
            if (!soundEntry)
            {
                sLog.outErrorDb("Quest %u has `SoundTurnInId` = %u but sound with that entry does not exists.",
                   qinfo->GetQuestId(), qinfo->SoundTurnInId);
                qinfo->SoundTurnInId = 0;
            }
        }

        if (qinfo->ExclusiveGroup)
        {
            m_ExclusiveQuestGroups.insert(ExclusiveQuestGroupsMap::value_type(qinfo->ExclusiveGroup, qinfo->GetQuestId()));
        }

        if (qinfo->LimitTime)
        {
            qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_TIMED);
        }
    }

    // check QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT for spell with SPELL_EFFECT_QUEST_COMPLETE
    for (uint32 i = 0; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(i);
        if (!spellInfo)
        {
            continue;
        }

        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(j));
            if (!spellEffect)
            {
                continue;
            }
            if (spellEffect->Effect != SPELL_EFFECT_QUEST_COMPLETE)
            {
                continue;
            }

            uint32 quest_id = spellEffect->EffectMiscValue_0;

            Quest const* quest = GetQuestTemplate(quest_id);

            // some quest referenced in spells not exist (outdated spells)
            if (!quest)
            {
                continue;
            }

            // Exclude false positive of quest 10162
            if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT) && spellInfo->ID != 33824 && quest_id != 10162)
            {
                sLog.outErrorDb("Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE for quest %u , but quest does not have SpecialFlags QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT (2) set. Quest SpecialFlags should be corrected to enable this objective.", spellInfo->ID, quest_id);

                // this will prevent quest completing without objective
                const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
            }
        }
    }

    sLog.outString(">> Loaded %zu quests definitions", mQuestTemplates.size());
    sLog.outString();
}

/**
 * @brief Loads localized quest texts and objective strings.
 */
void ObjectMgr::LoadQuestLocales()
{
    mQuestLocaleMap.clear();                                // need for reload case

    std::vector<QuestLocaleRow> rows = QuestDataRepository::QueryQuestLocales();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Quest locale strings. DB table `locales_quest` is empty.");
        return;
    }

    BarGoLink bar(rows.size());

    for (QuestLocaleRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        if (!GetQuestTemplate(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_quest` has data for nonexistent quest entry %u, skipped.", entry);
            continue;
        }

        QuestLocale& data = mQuestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = row.title[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Title.size() <= idx)
                    {
                        data.Title.resize(idx + 1);
                    }

                    data.Title[idx] = str;
                }
            }
            str = row.details[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Details.size() <= idx)
                    {
                        data.Details.resize(idx + 1);
                    }

                    data.Details[idx] = str;
                }
            }
            str = row.objectives[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Objectives.size() <= idx)
                    {
                        data.Objectives.resize(idx + 1);
                    }

                    data.Objectives[idx] = str;
                }
            }
            str = row.offerRewardText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OfferRewardText.size() <= idx)
                    {
                        data.OfferRewardText.resize(idx + 1);
                    }

                    data.OfferRewardText[idx] = str;
                }
            }
            str = row.requestItemsText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.RequestItemsText.size() <= idx)
                    {
                        data.RequestItemsText.resize(idx + 1);
                    }

                    data.RequestItemsText[idx] = str;
                }
            }
            str = row.endText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.EndText.size() <= idx)
                    {
                        data.EndText.resize(idx + 1);
                    }

                    data.EndText[idx] = str;
                }
            }
            str = row.completedText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.CompletedText.size() <= idx)
                    {
                        data.CompletedText.resize(idx + 1);
                    }

                    data.CompletedText[idx] = str;
                }
            }
            for (int k = 0; k < 4; ++k)
            {
                str = row.objectiveText[i - 1][k];
                if (!str.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.ObjectiveText[k].size() <= idx)
                        {
                            data.ObjectiveText[k].resize(idx + 1);
                        }

                        data.ObjectiveText[k][idx] = str;
                    }
                }
            }
            str = row.portraitGiverName[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.PortraitGiverName.size() <= idx)
                    {
                        data.PortraitGiverName.resize(idx + 1);
                    }

                    data.PortraitGiverName[idx] = str;
                }
            }
            str = row.portraitGiverText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.PortraitGiverText.size() <= idx)
                    {
                        data.PortraitGiverText.resize(idx + 1);
                    }

                    data.PortraitGiverText[idx] = str;
                }
            }
            str = row.portraitTurnInName[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.PortraitTurnInName.size() <= idx)
                    {
                        data.PortraitTurnInName.resize(idx + 1);
                    }

                    data.PortraitTurnInName[idx] = str;
                }
            }
            str = row.portraitTurnInText[i - 1];
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.PortraitTurnInText.size() <= idx)
                    {
                        data.PortraitTurnInText.resize(idx + 1);
                    }

                    data.PortraitTurnInText[idx] = str;
                }
            }
        }
    }

    sLog.outString();
    sLog.outString(">> Loaded %zu Quest locale strings", mQuestLocaleMap.size());
}
