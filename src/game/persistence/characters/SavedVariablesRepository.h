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

#ifndef MANGOS_H_SAVEDVARIABLESREPOSITORY
#define MANGOS_H_SAVEDVARIABLESREPOSITORY

#include "Platform/Define.h"

#include <optional>

class QueryResult;

/// The realm's saved variables (`saved_variables`). The startup reads run synchronously; every
/// write is queued on the character database.
namespace SavedVariablesRepository
{
    /// Selects the next automatic arena point distribution time.
    std::optional<uint64> QueryNextArenaPointDistribution();

    /// Inserts the next automatic arena point distribution time.
    void InsertNextArenaPointDistribution(uint64 time);

    /// Each selects one of the next reset times, at start-up.
    QueryResult* QueryNextWeeklyQuestReset();

    /// The daily quests'.
    QueryResult* QueryNextDailyQuestReset();

    /// The monthly quests'.
    QueryResult* QueryNextMonthlyQuestReset();

    /// The currencies'.
    QueryResult* QueryNextCurrencyReset();

    /// The random battleground's.
    QueryResult* QueryNextRandomBGReset();

    /// Each inserts one of the next reset times when there is none.
    void InsertNextWeeklyQuestReset(uint64 time);

    /// The daily quests'.
    void InsertNextDailyQuestReset(uint64 time);

    /// The currencies'.
    void InsertNextCurrencyReset(uint64 time);

    /// The random battleground's.
    void InsertNextRandomBGReset(uint64 time);

    /// Each writes one of the next reset times over the saved row.
    void SetNextMonthlyQuestReset(uint64 time);

    /// The daily quests'.
    void SetNextDailyQuestReset(uint64 time);

    /// The weekly quests'.
    void SetNextWeeklyQuestReset(uint64 time);

    /// The currencies'.
    void SetNextCurrencyReset(uint64 time);
}

#endif
