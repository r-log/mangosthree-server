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

#ifndef MANGOS_H_QUESTSTATUSREPOSITORY
#define MANGOS_H_QUESTSTATUSREPOSITORY

#include <set>
#include <vector>
#include "Platform/Define.h"

class Field;
class SqlQueryHolder;
class QueryResult;

/// The objective counts a saved quest status row holds: four creature or game object counts and
/// six item counts.
enum
{
    QUEST_STATUS_ROW_OBJECTIVES = 4,
    QUEST_STATUS_ROW_ITEM_OBJECTIVES = 6
};

/// One saved quest status row as the login reads it: the quest, the status, the rewarded and
/// explored flags, the absolute timer end, and the objective counts.
struct QuestStatusRow
{
    uint32 quest = 0;
    uint32 status = 0;
    uint8 rewarded = 0;
    uint8 explored = 0;
    uint64 timer = 0;
    uint32 creatureOrGoCount[QUEST_STATUS_ROW_OBJECTIVES] = {};
    uint32 itemCount[QUEST_STATUS_ROW_ITEM_OBJECTIVES] = {};
};

/// One quest status row as the save writes it.
struct QuestStatusSaveRow
{
    uint32 quest = 0;
    uint8 status = 0;
    uint8 rewarded = 0;
    uint8 explored = 0;
    uint64 timer = 0;
    uint32 creatureOrGoCount[QUEST_STATUS_ROW_OBJECTIVES] = {};
    uint32 itemCount[QUEST_STATUS_ROW_ITEM_OBJECTIVES] = {};
};

/// The character's saved quest status, and its weekly and monthly quests. Every write is queued
/// on the character database.
namespace QuestStatusRepository
{
    /// Reads one row of the login holder's quest status result: quest, status, rewarded,
    /// explored, timer, mobcount1..4, itemcount1..6.
    QuestStatusRow ReadRow(Field* fields);

    /// Reads a holder slot's rows, its columns in ReadRow's order.
    std::vector<QuestStatusRow> ReadHolder(SqlQueryHolder* holder, size_t slot);

    /// Reads the quest id of every row of the login holder's weekly or monthly result, in order,
    /// and deletes the result; none for no result.
    std::vector<uint32> ReadQuestIds(QueryResult* result);

    /// Reads a holder slot's quest ids, as ReadQuestIds does.
    std::vector<uint32> ReadHolderQuestIds(SqlQueryHolder* holder, size_t slot);

    /// Inserts the owner's row of a new quest status.
    void Insert(uint32 ownerGuidLow, QuestStatusSaveRow const& row);

    /// Updates the owner's row of a changed quest status.
    void Update(uint32 ownerGuidLow, QuestStatusSaveRow const& row);

    /// Replaces the owner's weekly quests: a DELETE of them all, then an INSERT per quest.
    void SaveWeekly(uint32 ownerGuidLow, std::set<uint32> const& quests);

    /// Replaces the owner's monthly quests: a DELETE of them all, then an INSERT per quest.
    void SaveMonthly(uint32 ownerGuidLow, std::set<uint32> const& quests);

    /// Replaces the owner's daily quests: a DELETE of them all, then an INSERT per quest, in order.
    void SaveDaily(uint32 ownerGuidLow, std::vector<uint32> const& quests);

    /// Deletes every character's daily quests, as the daily reset does.
    void DeleteEveryDaily();

    /// Deletes every character's weekly quests, as the weekly reset does.
    void DeleteEveryWeekly();

    /// Empties every character's monthly quests, as the monthly reset does.
    void TruncateMonthly();
}

#endif
