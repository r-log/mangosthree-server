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

#include "QuestStatusMgr.h"
#include "Common/TimeConstants.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Utilities/Errors.h"

#include <cstdlib>

// Every body below moved here in decoupling D4a from the character object's quest, load and
// save files (the acceptance rules came in D4b; their own note says what changed in them).
// The statements, their order and their values are the old ones; what changed is
// where the inputs come from:
// the quest template through `lookup` (it was sObjectMgr.GetQuestTemplate), the game time
// through `now` (it was sWorld.GetGameTime(), which does not move inside a tick), and the
// character's guid and name through parameters (they were the owner's own accessors).

QuestStatusMgr::QuestStatusMgr()
    : m_weeklyChanged(false), m_monthlyChanged(false)
{
}

QuestStatusData const* QuestStatusMgr::Find(uint32 questId) const
{
    QuestStatusMap::const_iterator itr = m_status.find(questId);
    return itr != m_status.end() ? &itr->second : NULL;
}

/**
 * @brief Checks whether a quest is currently active in the quest log.
 *
 * @param quest_id The quest identifier to check.
 * @return True if the quest is active; otherwise, false.
 */
bool QuestStatusMgr::IsActiveQuest(uint32 quest_id) const
{
    QuestStatusMap::const_iterator itr = m_status.find(quest_id);

    return itr != m_status.end() && itr->second.m_status != QUEST_STATUS_NONE;
}

/**
 * @brief Checks whether a quest is currently active with a specific completion state.
 *
 * @param quest_id The quest identifier to check.
 * @param completed_or_not The completion-state filter.
 * @return True if the quest matches the requested state; otherwise, false.
 */
bool QuestStatusMgr::IsCurrentQuest(uint32 quest_id, uint8 completed_or_not) const
{
    QuestStatusMap::const_iterator itr = m_status.find(quest_id);
    if (itr == m_status.end())
    {
        return false;
    }

    QuestStatusData const& questStatus = itr->second;

    switch (completed_or_not)
    {
        case 1:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE;
        case 2:
            return questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded;
        default:
            return questStatus.m_status == QUEST_STATUS_INCOMPLETE || (questStatus.m_status == QUEST_STATUS_COMPLETE && !questStatus.m_rewarded);
    }
}

/**
 * @brief Gets the current status for a quest.
 *
 * @param quest_id The quest identifier to query.
 * @return The current quest status.
 */
QuestStatus QuestStatusMgr::GetQuestStatus(uint32 quest_id) const
{
    if (quest_id)
    {
        QuestStatusMap::const_iterator itr = m_status.find(quest_id);
        if (itr != m_status.end())
        {
            if (itr->second.m_status == QUEST_STATUS_FORCE_COMPLETE)
            {
                return QUEST_STATUS_COMPLETE;
            }
            return itr->second.m_status;
        }
    }
    return QUEST_STATUS_NONE;
}

/**
 * @brief Checks whether a quest reward has already been claimed.
 *
 * @param quest_id The quest identifier to query.
 * @param lookup The quest template lookup.
 * @return True if the quest reward is marked as claimed; otherwise, false.
 */
bool QuestStatusMgr::GetQuestRewardStatus(uint32 quest_id, TemplateLookup const& lookup) const
{
    Quest const* qInfo = lookup(quest_id);
    if (qInfo)
    {
        // for repeatable quests: rewarded field is set after first reward only to prevent getting XP more than once
        QuestStatusMap::const_iterator itr = m_status.find(quest_id);
        if (itr != m_status.end() && itr->second.m_status != QUEST_STATUS_NONE
            && !qInfo->IsRepeatable())
        {
            return itr->second.m_rewarded;
        }

        return false;
    }
    return false;
}

/**
 * @brief Checks whether a quest can currently be shared with other players.
 *
 * @param quest_id The quest identifier to query.
 * @param lookup The quest template lookup.
 * @return True if the quest is active and sharable; otherwise, false.
 */
bool QuestStatusMgr::CanShareQuest(uint32 quest_id, TemplateLookup const& lookup) const
{
    if (Quest const* qInfo = lookup(quest_id))
        if (qInfo->HasQuestFlag(QUEST_FLAGS_SHARABLE))
        {
            return IsCurrentQuest(quest_id);
        }

    return false;
}

// not used in MaNGOS, but used in scripting code
uint32 QuestStatusMgr::GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry, TemplateLookup const& lookup)
{
    Quest const* qInfo = lookup(quest_id);
    if (!qInfo)
    {
        return 0;
    }

    for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        if (qInfo->ReqCreatureOrGOId[j] == entry)
        {
            return m_status[quest_id].m_creatureOrGOcount[j];
        }

    return 0;
}

bool QuestStatusMgr::SatisfyQuestWeek(Quest const* qInfo) const
{
    if (!qInfo->IsWeekly() || m_weeklyQuests.empty())
    {
        return true;
    }

    // if not found in cooldown list
    return m_weeklyQuests.find(qInfo->GetQuestId()) == m_weeklyQuests.end();
}

bool QuestStatusMgr::SatisfyQuestMonth(Quest const* qInfo) const
{
    if (!qInfo->IsMonthly() || m_monthlyQuests.empty())
    {
        return true;
    }

    // if not found in cooldown list
    return m_monthlyQuests.find(qInfo->GetQuestId()) == m_monthlyQuests.end();
}

// The acceptance rules below moved here in decoupling D4b from the character object's quest
// file. Each body is the old one with every "send the cannot-take response if asked, then
// return false" replaced by returning a failed verdict with that reason, and every "return
// true" by a satisfied verdict; the owner's wrapper sends the response. The reads, lookups
// and early returns are in the old order. The inputs that came from global stores are the
// parameters: `lookup` (was sObjectMgr.GetQuestTemplate), `groups` (was
// sObjectMgr.GetExclusiveQuestGroupsMapBounds) and `dailyCheck` (was the owner's
// SatisfyQuestDay(quest, false)).

/**
 * @brief Checks whether the quest is not already active in the quest log.
 *
 * @param qInfo The quest to validate.
 * @return Satisfied, or failed with INVALIDREASON_QUEST_ALREADY_ON.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestStatus(Quest const* qInfo) const
{
    QuestStatusMap::const_iterator itr = m_status.find(qInfo->GetQuestId());

    if (itr != m_status.end() && itr->second.m_status != QUEST_STATUS_NONE)
    {
        return QuestVerdict::Failed(INVALIDREASON_QUEST_ALREADY_ON);
    }

    return QuestVerdict::Satisfied();
}

/**
 * @brief Checks whether another timed quest may be accepted.
 *
 * @param qInfo The quest to validate.
 * @return Satisfied, or failed with INVALIDREASON_QUEST_ONLY_ONE_TIMED.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestTimed(Quest const* qInfo) const
{
    if (!m_timedQuests.empty() && qInfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED))
    {
        return QuestVerdict::Failed(INVALIDREASON_QUEST_ONLY_ONE_TIMED);
    }

    return QuestVerdict::Satisfied();
}

/**
 * @brief Checks whether exclusive-group rules allow the quest to be accepted.
 *
 * @param qInfo The quest to validate.
 * @param lookup The quest template lookup.
 * @param groups The exclusive-group lookup.
 * @param dailyCheck The owner's daily rule, asked without a response.
 * @return Satisfied, or failed with INVALIDREASON_DONT_HAVE_REQ.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestExclusiveGroup(Quest const* qInfo, TemplateLookup const& lookup,
                                                        ExclusiveGroupLookup const& groups, DailyCheck const& dailyCheck) const
{
    // non positive exclusive group, if > 0 then can be start if any other quest in exclusive group already started/completed
    if (qInfo->GetExclusiveGroup() <= 0)
    {
        return QuestVerdict::Satisfied();
    }

    ExclusiveGroupBounds bounds = groups(qInfo->GetExclusiveGroup());

    MANGOS_ASSERT(bounds.first != bounds.second);           // must always be found if qInfo->ExclusiveGroup != 0

    for (ExclusiveGroupMap::const_iterator iter = bounds.first; iter != bounds.second; ++iter)
    {
        uint32 exclude_Id = iter->second;

        // skip checked quest id, only state of other quests in group is interesting
        if (exclude_Id == qInfo->GetQuestId())
        {
            continue;
        }

        // not allow have daily quest if daily quest from exclusive group already recently completed
        Quest const* Nquest = lookup(exclude_Id);
        if (!dailyCheck(Nquest) || !SatisfyQuestWeek(Nquest))
        {
            return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
        }

        QuestStatusMap::const_iterator i_exstatus = m_status.find(exclude_Id);

        // alternative quest already started or completed
        if (i_exstatus != m_status.end() &&
           (i_exstatus->second.m_status == QUEST_STATUS_COMPLETE || i_exstatus->second.m_status == QUEST_STATUS_INCOMPLETE))
        {
            return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
        }
    }

    return QuestVerdict::Satisfied();
}

/**
 * @brief Checks whether later quests in the chain do not block acceptance.
 *
 * @param qInfo The quest to validate.
 * @return Satisfied, or failed with INVALIDREASON_DONT_HAVE_REQ.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestNextChain(Quest const* qInfo) const
{
    if (!qInfo->GetNextQuestInChain())
    {
        return QuestVerdict::Satisfied();
    }

    // next quest in chain already started or completed
    QuestStatusMap::const_iterator itr = m_status.find(qInfo->GetNextQuestInChain());
    if (itr != m_status.end() &&
       (itr->second.m_status == QUEST_STATUS_COMPLETE || itr->second.m_status == QUEST_STATUS_INCOMPLETE))
    {
        return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
    }

    // check for all quests further up the chain
    // only necessary if there are quest chains with more than one quest that can be skipped
    // return SatisfyQuestNextChain( qInfo->GetNextQuestInChain(), msg );
    return QuestVerdict::Satisfied();
}

/**
 * @brief Checks whether previous-chain quests do not block acceptance.
 *
 * @param qInfo The quest to validate.
 * @return Satisfied, or failed with INVALIDREASON_DONT_HAVE_REQ.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestPrevChain(Quest const* qInfo) const
{
    // No previous quest in chain
    if (qInfo->prevChainQuests.empty())
    {
        return QuestVerdict::Satisfied();
    }

    for (Quest::PrevChainQuests::const_iterator iter = qInfo->prevChainQuests.begin(); iter != qInfo->prevChainQuests.end(); ++iter)
    {
        uint32 prevId = *iter;

        // If any of the previous quests in chain active, return false
        if (IsCurrentQuest(prevId))
        {
            return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
        }

        // check for all quests further down the chain
        // only necessary if there are quest chains with more than one quest that can be skipped
        // if ( !SatisfyQuestPrevChain( prevId, msg ) )
        //    return false;
    }

    // No previous quest in chain active
    return QuestVerdict::Satisfied();
}

/**
 * @brief Checks whether previous-quest requirements for a quest are satisfied.
 *
 * @param qInfo The quest to validate.
 * @param lookup The quest template lookup.
 * @param groups The exclusive-group lookup.
 * @return Satisfied, or failed with INVALIDREASON_DONT_HAVE_REQ.
 */
QuestVerdict QuestStatusMgr::SatisfyQuestPreviousQuest(Quest const* qInfo, TemplateLookup const& lookup,
                                                       ExclusiveGroupLookup const& groups) const
{
    // No previous quest (might be first quest in a series)
    if (qInfo->prevQuests.empty())
    {
        return QuestVerdict::Satisfied();
    }

    for (Quest::PrevQuests::const_iterator iter = qInfo->prevQuests.begin(); iter != qInfo->prevQuests.end(); ++iter)
    {
        uint32 prevId = abs(*iter);

        QuestStatusMap::const_iterator i_prevstatus = m_status.find(prevId);
        Quest const* qPrevInfo = lookup(prevId);

        if (qPrevInfo && i_prevstatus != m_status.end())
        {
            // If any of the positive previous quests completed, return true
            if (*iter > 0 && i_prevstatus->second.m_rewarded)
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                {
                    return QuestVerdict::Satisfied();
                }

                // each-from-all exclusive group ( < 0)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != qInfo->GetPrevQuestId())
                {
                    return QuestVerdict::Satisfied();
                }

                // can be start if only all quests in prev quest exclusive group completed and rewarded
                ExclusiveGroupBounds bounds = groups(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0

                for (ExclusiveGroupMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting
                    if (exclude_Id == prevId)
                    {
                        continue;
                    }

                    QuestStatusMap::const_iterator i_exstatus = m_status.find(exclude_Id);

                    // alternative quest from group also must be completed and rewarded(reported)
                    if (i_exstatus == m_status.end() || !i_exstatus->second.m_rewarded)
                    {
                        return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
                    }
                }
                return QuestVerdict::Satisfied();
            }
            // If any of the negative previous quests active, return true
            if (*iter < 0 && IsCurrentQuest(prevId))
            {
                // skip one-from-all exclusive group
                if (qPrevInfo->GetExclusiveGroup() >= 0)
                {
                    return QuestVerdict::Satisfied();
                }

                // each-from-all exclusive group ( < 0)
                // given a group with 2+ quests, and one of those has a branch that is not restricted by the group, return true
                if (qInfo->GetPrevQuestId() != 0 && qPrevInfo->GetNextQuestId() != abs(qInfo->GetPrevQuestId()))
                {
                    return QuestVerdict::Satisfied();
                }

                // each-from-all exclusive group ( < 0)
                // can be start if only all quests in prev quest exclusive group active
                ExclusiveGroupBounds bounds = groups(qPrevInfo->GetExclusiveGroup());

                MANGOS_ASSERT(bounds.first != bounds.second); // always must be found if qPrevInfo->ExclusiveGroup != 0

                for (ExclusiveGroupMap::const_iterator iter2 = bounds.first; iter2 != bounds.second; ++iter2)
                {
                    uint32 exclude_Id = iter2->second;

                    // skip checked quest id, only state of other quests in group is interesting
                    if (exclude_Id == prevId)
                    {
                        continue;
                    }

                    // alternative quest from group also must be active
                    if (!IsCurrentQuest(exclude_Id))
                    {
                        return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
                    }
                }
                return QuestVerdict::Satisfied();
            }
        }
    }

    // Has only positive prev. quests in non-rewarded state
    // and negative prev. quests in non-active state
    return QuestVerdict::Failed(INVALIDREASON_DONT_HAVE_REQ);
}

/**
 * @brief Updates the stored status for a quest.
 *
 * @param quest_id The quest identifier to update.
 * @param status The new quest status.
 * @param lookup The quest template lookup.
 */
void QuestStatusMgr::SetQuestStatus(uint32 quest_id, QuestStatus status, TemplateLookup const& lookup)
{
    if (lookup(quest_id))
    {
        QuestStatusData& q_status = m_status[quest_id];

        q_status.m_status = status;

        if (q_status.uState != QUEST_NEW)
        {
            q_status.uState = QUEST_CHANGED;
        }
    }
}

void QuestStatusMgr::SetWeeklyQuestStatus(uint32 quest_id)
{
    m_weeklyQuests.insert(quest_id);
    m_weeklyChanged = true;
}

void QuestStatusMgr::SetMonthlyQuestStatus(uint32 quest_id)
{
    m_monthlyQuests.insert(quest_id);
    m_monthlyChanged = true;
}

void QuestStatusMgr::ResetWeeklyQuestStatus()
{
    if (m_weeklyQuests.empty())
    {
        return;
    }

    m_weeklyQuests.clear();
    // DB data deleted in caller
    m_weeklyChanged = false;
}

void QuestStatusMgr::ResetMonthlyQuestStatus()
{
    if (m_monthlyQuests.empty())
    {
        return;
    }

    m_monthlyQuests.clear();
    // DB data deleted in caller
    m_monthlyChanged = false;
}

/**
 * @brief The fill half of one quest status row.
 *
 * @param fields The row: quest, status, rewarded, explored, timer, mobcount1..4, itemcount1..6.
 * @param now The game time the timer is measured against.
 * @param ownerName The character's name, for the invalid-status log line.
 * @param lookup The quest template lookup.
 * @return Whether the row was taken, its quest and the time its quest-log slot carries.
 */
QuestRowResult QuestStatusMgr::FillRow(Field* fields, time_t now, char const* ownerName, TemplateLookup const& lookup)
{
    QuestRowResult row;
    row.accepted = false;
    row.questId = fields[0].GetUInt32();
    row.quest = NULL;
    row.slotTime = 0;

    uint32 quest_id = row.questId;
    // used to be new, no delete?
    Quest const* pQuest = lookup(quest_id);
    if (!pQuest)
    {
        return row;
    }

    // find or create
    QuestStatusData& questStatusData = m_status[quest_id];

    uint32 qstatus = fields[1].GetUInt32();
    if (qstatus < MAX_QUEST_STATUS)
    {
        questStatusData.m_status = QuestStatus(qstatus);
    }
    else
    {
        questStatusData.m_status = QUEST_STATUS_NONE;
        sLog.outError("Player %s have invalid quest %d status (%d), replaced by QUEST_STATUS_NONE(0).", ownerName, quest_id, qstatus);
    }

    questStatusData.m_rewarded = (fields[2].GetUInt8() > 0);
    questStatusData.m_explored = (fields[3].GetUInt8() > 0);

    time_t quest_time = time_t(fields[4].GetUInt64());

    if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && !GetQuestRewardStatus(quest_id, lookup) && questStatusData.m_status != QUEST_STATUS_NONE)
    {
        AddTimedQuest(quest_id);

        if (quest_time <= now)
        {
            questStatusData.m_timer = 1;
        }
        else
        {
            questStatusData.m_timer = uint32(quest_time - now) * IN_MILLISECONDS;
        }
    }
    else
    {
        quest_time = 0;
    }

    questStatusData.m_creatureOrGOcount[0] = fields[5].GetUInt32();
    questStatusData.m_creatureOrGOcount[1] = fields[6].GetUInt32();
    questStatusData.m_creatureOrGOcount[2] = fields[7].GetUInt32();
    questStatusData.m_creatureOrGOcount[3] = fields[8].GetUInt32();
    questStatusData.m_itemcount[0] = fields[9].GetUInt32();
    questStatusData.m_itemcount[1] = fields[10].GetUInt32();
    questStatusData.m_itemcount[2] = fields[11].GetUInt32();
    questStatusData.m_itemcount[3] = fields[12].GetUInt32();
    questStatusData.m_itemcount[4] = fields[13].GetUInt32();
    questStatusData.m_itemcount[5] = fields[14].GetUInt32();

    questStatusData.uState = QUEST_UNCHANGED;

    row.accepted = true;
    row.quest = pQuest;
    row.slotTime = uint32(quest_time);
    return row;
}

void QuestStatusMgr::LoadWeekly(QueryResult* result, uint32 guidLow, TemplateLookup const& lookup)
{
    m_weeklyQuests.clear();

    // `result` is the login holder's `SELECT quest FROM character_queststatus_weekly WHERE guid = ...`

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = lookup(quest_id);
            if (!pQuest)
            {
                continue;
            }

            m_weeklyQuests.insert(quest_id);

            DEBUG_LOG("Weekly quest {%u} cooldown for player (GUID: %u)", quest_id, guidLow);
        }
        while (result->NextRow());

        delete result;
    }
    m_weeklyChanged = false;
}

void QuestStatusMgr::LoadMonthly(QueryResult* result, uint32 guidLow, TemplateLookup const& lookup)
{
    m_monthlyQuests.clear();

    // `result` is the login holder's `SELECT quest FROM character_queststatus_monthly WHERE guid = ...`

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();

            Quest const* pQuest = lookup(quest_id);
            if (!pQuest)
            {
                continue;
            }

            m_monthlyQuests.insert(quest_id);

            DEBUG_LOG("Monthly quest {%u} cooldown for player (GUID: %u)", quest_id, guidLow);
        }
        while (result->NextRow());

        delete result;
    }

    m_monthlyChanged = false;
}

/**
 * @brief Saves tracked quest status progress to the database.
 *
 * @param guidLow The character's guid.
 * @param now The game time the timer is stored against.
 */
void QuestStatusMgr::SaveStatus(uint32 guidLow, time_t now)
{
    static SqlStatementID insertQuestStatus ;

    static SqlStatementID updateQuestStatus ;

    // we don't need transactions here.
    for (QuestStatusMap::iterator i = m_status.begin(); i != m_status.end(); ++i)
    {
        QuestStatusData &questStatus = i->second;
        switch (questStatus.uState)
        {
            case QUEST_NEW :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(insertQuestStatus, "INSERT INTO `character_queststatus` (`guid`,`quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`,`itemcount5`,`itemcount6`) "
                                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

                stmt.addUInt32(guidLow);
                stmt.addUInt32(i->first);
                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + now));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_ITEM_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.Execute();
            }
            break;
            case QUEST_CHANGED :
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updateQuestStatus, "UPDATE `character_queststatus` SET `status` = ?,`rewarded` = ?,`explored` = ?,`timer` = ?,"
                                    "`mobcount1` = ?,`mobcount2` = ?,`mobcount3` = ?,`mobcount4` = ?,`itemcount1` = ?,`itemcount2` = ?,`itemcount3` = ?,`itemcount4` = ?,`itemcount5` = ?,`itemcount6` = ? WHERE `guid` = ? AND `quest` = ?");

                stmt.addUInt8(questStatus.m_status);
                stmt.addUInt8(questStatus.m_rewarded);
                stmt.addUInt8(questStatus.m_explored);
                stmt.addUInt64(uint64(questStatus.m_timer / IN_MILLISECONDS + now));
                for (int k = 0; k < QUEST_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_creatureOrGOcount[k]);
                }
                for (int k = 0; k < QUEST_ITEM_OBJECTIVES_COUNT; ++k)
                {
                    stmt.addUInt32(questStatus.m_itemcount[k]);
                }
                stmt.addUInt32(guidLow);
                stmt.addUInt32(i->first);
                stmt.Execute();
            }
            break;
            case QUEST_UNCHANGED:
                break;
        };
        questStatus.uState = QUEST_UNCHANGED;
    }
}

void QuestStatusMgr::SaveWeekly(uint32 guidLow)
{
    if (!m_weeklyChanged || m_weeklyQuests.empty())
    {
        return;
    }

    // we don't need transactions here.
    static SqlStatementID delQuestStatus ;
    static SqlStatementID insQuestStatus  ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus, "DELETE FROM `character_queststatus_weekly` WHERE `guid` = ?");
    SqlStatement stmtIns =  CharacterDatabase.CreateStatement(insQuestStatus, "INSERT INTO `character_queststatus_weekly` (`guid`,`quest`) VALUES (?, ?)");

    stmtDel.PExecute(guidLow);

    for (QuestSet::const_iterator iter = m_weeklyQuests.begin(); iter != m_weeklyQuests.end(); ++iter)
    {
        uint32 quest_id  = *iter;
        stmtIns.PExecute(guidLow, quest_id);
    }

    m_weeklyChanged = false;
}

void QuestStatusMgr::SaveMonthly(uint32 guidLow)
{
    if (!m_monthlyChanged || m_monthlyQuests.empty())
    {
        return;
    }

    // we don't need transactions here.
    static SqlStatementID deleteQuest ;
    static SqlStatementID insertQuest ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(deleteQuest, "DELETE FROM `character_queststatus_monthly` WHERE `guid` = ?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insertQuest, "INSERT INTO `character_queststatus_monthly` (`guid`, `quest`) VALUES (?, ?)");

    stmtDel.PExecute(guidLow);

    for (QuestSet::const_iterator iter = m_monthlyQuests.begin(); iter != m_monthlyQuests.end(); ++iter)
    {
        uint32 quest_id = *iter;
        stmtIns.PExecute(guidLow, quest_id);
    }

    m_monthlyChanged = false;
}
