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

#include <vector>

class QueryResult;

/// One `quest_relations` row.
struct QuestRelationRow
{
    uint32 entry = 0;
    uint32 quest = 0;
};

/// The quest data on the world database: `ObjectMgr`'s quest, quest relation and quest point of
/// interest loads at startup and on a reload, each run synchronously.
namespace QuestDataRepository
{
    /// Selects every quest (`quest_template`).
    QueryResult* QueryQuests();

    /// Selects every quest's localised texts (`locales_quest`).
    QueryResult* QueryQuestLocales();

    /// Selects the quest relations of one actor and role.
    std::vector<QuestRelationRow> QueryQuestRelations(int actor, int role);

    /// Selects every quest relation of one actor, whatever its role.
    std::vector<QuestRelationRow> QueryActorQuestRelations(int actor);

    /// Selects every quest point of interest (`quest_poi`).
    QueryResult* QueryQuestPois();

    /// Selects every quest point of interest's points (`quest_poi_points`).
    QueryResult* QueryQuestPoiPoints();
}

#endif
