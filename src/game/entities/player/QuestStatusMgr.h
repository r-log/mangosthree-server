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

#ifndef MANGOS_H_QUESTSTATUSMGR
#define MANGOS_H_QUESTSTATUSMGR

#include "QuestDef.h"

#include <ctime>
#include <functional>
#include <map>
#include <set>

/**
 * @file QuestStatusMgr.h
 * @brief Decoupling D4a: one character's quest status, held apart from the object that plays it.
 *
 * The state is the per-quest status map, the timed/weekly/monthly quest sets and the two
 * "changed since the last save" flags of the weekly and monthly sets. The object carries no
 * owner: everything else it needs -- the quest templates, the current game time, the
 * character's guid and name -- is handed in at the call, so `mangos_tests` can build one from
 * nothing and drive it with fake rows.
 *
 * What stays with the owning player object, and why: the daily quests (they live in update
 * fields the client sees), the quest-log slots (update fields), learning a rewarded quest's
 * spells, titles and talent points (spells, update fields), and the world-object refresh after
 * a status change (packets).
 *
 * THE LOAD IS PER ROW. `FillRow` does the fill half of ONE `character_queststatus` row and the
 * owner applies that row (slot writes, spells, title, talent points) before it moves to the
 * next one. It must stay that way: applying a rewarded row casts spells, and a quest-complete
 * spell effect reaches the quest-credit code, which reads and writes the state of a LATER row.
 * A load that filled every row first and applied them afterwards would change what that code
 * sees.
 *
 * KEPT SEMANTICS. `Entry()` inserts a default row (status NONE, `QUEST_NEW`) for a quest the
 * map does not hold yet, exactly as the `operator[]` it replaces did; the next `SaveStatus`
 * INSERTs that row. `SaveWeekly`/`SaveMonthly` return early on an empty set even when the set
 * changed, so no DELETE is issued then. Both are pre-existing behaviour, kept on purpose.
 *
 * THE ONE DEFINED UB. The monthly "changed" flag used to be left uninitialised until the
 * monthly load ran, and a save before any load read it. Both flags start `false` here.
 */

class QueryResult;
class Field;
class Quest;

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

/// What `QuestStatusMgr::FillRow` did with one `character_queststatus` row.
struct QuestRowResult
{
    bool accepted;          ///< false: the quest has no template and the row was skipped
    uint32 questId;
    Quest const* quest;     ///< the template; NULL when not accepted
    uint32 slotTime;        ///< what the quest-log slot is written with: the row's absolute
                            ///< expiry time for a running timed quest, otherwise 0
};

class QuestStatusMgr
{
    public:
        typedef std::function<Quest const*(uint32)> TemplateLookup;
        typedef std::set<uint32> QuestSet;

        QuestStatusMgr();

        /*** state ***/

        QuestStatusMap& Map() { return m_status; }
        QuestStatusMap const& Map() const { return m_status; }

        /// Find or create: a missing id gets a default row (status NONE, `QUEST_NEW`).
        QuestStatusData& Entry(uint32 questId) { return m_status[questId]; }
        /// NULL when the map holds no row for the id. Never inserts.
        QuestStatusData const* Find(uint32 questId) const;

        QuestSet const& TimedQuests() const { return m_timedQuests; }
        QuestSet const& WeeklyQuests() const { return m_weeklyQuests; }
        QuestSet const& MonthlyQuests() const { return m_monthlyQuests; }
        bool IsWeeklyChanged() const { return m_weeklyChanged; }
        bool IsMonthlyChanged() const { return m_monthlyChanged; }

        /*** queries ***/

        bool IsActiveQuest(uint32 quest_id) const;
        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const;
        QuestStatus GetQuestStatus(uint32 quest_id) const;
        bool GetQuestRewardStatus(uint32 quest_id, TemplateLookup const& lookup) const;
        bool CanShareQuest(uint32 quest_id, TemplateLookup const& lookup) const;
        /// Non-const on purpose: an unknown row is created, as before.
        uint32 GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry, TemplateLookup const& lookup);
        bool SatisfyQuestWeek(Quest const* qInfo) const;
        bool SatisfyQuestMonth(Quest const* qInfo) const;

        /*** writes ***/

        /// The state half of the old status setter: sets the status when the quest has a
        /// template. The owner refreshes the world objects afterwards, as before.
        void SetQuestStatus(uint32 quest_id, QuestStatus status, TemplateLookup const& lookup);
        void AddTimedQuest(uint32 quest_id) { m_timedQuests.insert(quest_id); }
        void RemoveTimedQuest(uint32 quest_id) { m_timedQuests.erase(quest_id); }
        void SetWeeklyQuestStatus(uint32 quest_id);
        void SetMonthlyQuestStatus(uint32 quest_id);
        void ResetWeeklyQuestStatus();
        void ResetMonthlyQuestStatus();

        /*** rows ***/

        /// Called where the status load starts, before the first row.
        void ClearStatus() { m_status.clear(); }
        /// The fill half of one `character_queststatus` row:
        /// `quest, status, rewarded, explored, timer, mobcount1..4, itemcount1..6`.
        /// `ownerName` is only used by the invalid-status log line.
        QuestRowResult FillRow(Field* fields, time_t now, char const* ownerName, TemplateLookup const& lookup);
        /// The whole weekly/monthly load (`SELECT quest ...`), deleting `result` as before.
        /// `guidLow` is only used by the debug log line.
        void LoadWeekly(QueryResult* result, uint32 guidLow, TemplateLookup const& lookup);
        void LoadMonthly(QueryResult* result, uint32 guidLow, TemplateLookup const& lookup);

        /// INSERT for every `QUEST_NEW` row, UPDATE for every `QUEST_CHANGED` row, in quest-id
        /// order; then every row is `QUEST_UNCHANGED`. Asynchronous statements, as before.
        void SaveStatus(uint32 guidLow, time_t now);
        void SaveWeekly(uint32 guidLow);
        void SaveMonthly(uint32 guidLow);

    private:
        QuestStatusMap m_status;
        QuestSet m_timedQuests;
        QuestSet m_weeklyQuests;
        QuestSet m_monthlyQuests;
        bool m_weeklyChanged;
        bool m_monthlyChanged;
};

#endif
