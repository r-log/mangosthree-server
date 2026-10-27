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

/// The realm's saved variables (`saved_variables`). The startup reads run synchronously; every
/// write is queued on the character database.
namespace SavedVariablesRepository
{
    /// Selects the next automatic arena point distribution time.
    std::optional<uint64> QueryNextArenaPointDistribution();

    /// Inserts the next automatic arena point distribution time.
    void InsertNextArenaPointDistribution(uint64 time);

    /// Each selects one of the next reset times, at start-up.
    std::optional<uint64> QueryNextWeeklyQuestReset();

    /// The daily quests'.
    std::optional<uint64> QueryNextDailyQuestReset();

    /// The monthly quests'.
    std::optional<uint64> QueryNextMonthlyQuestReset();

    /// The currencies'.
    std::optional<uint64> QueryNextCurrencyReset();

    /// The random battleground's.
    std::optional<uint64> QueryNextRandomBGReset();

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

    /// Selects the cleanups the character database is due, at start-up.
    std::optional<uint32> QueryCleaningFlags();

    /// Selects the DBC build the character database was last cleaned for, none when the column
    /// is missing.
    std::optional<uint32> QueryLastCleanedDbcBuild();

    /// Each clears the cleanups due once a cleanup has run: with the DBC build it ran for, and
    /// without it when the database has no column for it.
    void FinishCleaning(uint32 dbcBuild);
    void FinishCleaningWithoutBuild();
}

#endif
