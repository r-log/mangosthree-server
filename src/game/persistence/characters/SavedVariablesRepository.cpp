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

#include "SavedVariablesRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace SavedVariablesRepository
{
    std::optional<uint64> QueryNextArenaPointDistribution()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextArenaPointDistributionTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    void InsertNextArenaPointDistribution(uint64 time)
    {
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextArenaPointDistributionTime`) VALUES ('" UI64FMTD "')", time);
    }

    std::optional<uint64> QueryNextWeeklyQuestReset()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextWeeklyQuestResetTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    std::optional<uint64> QueryNextDailyQuestReset()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextDailyQuestResetTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    std::optional<uint64> QueryNextMonthlyQuestReset()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextMonthlyQuestResetTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    std::optional<uint64> QueryNextCurrencyReset()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextCurrenciesResetTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    std::optional<uint64> QueryNextRandomBGReset()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `NextRandomBGResetTime` FROM `saved_variables`");
        return ReadRow<uint64>(result, [](Field* fields) { return fields[0].GetUInt64(); });
    }

    void InsertNextWeeklyQuestReset(uint64 time)
    {
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextWeeklyQuestResetTime`) VALUES ('" UI64FMTD "')", time);
    }

    void InsertNextDailyQuestReset(uint64 time)
    {
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextDailyQuestResetTime`) VALUES ('" UI64FMTD "')", time);
    }

    void InsertNextCurrencyReset(uint64 time)
    {
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextCurrenciesResetTime`) VALUES ('" UI64FMTD "')", time);
    }

    void InsertNextRandomBGReset(uint64 time)
    {
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextRandomBGResetTime`) VALUES ('" UI64FMTD "')", time);
    }

    void SetNextMonthlyQuestReset(uint64 time)
    {
        CharacterDatabase.PExecute("UPDATE `saved_variables` SET `NextMonthlyQuestResetTime` = '" UI64FMTD "'", time);
    }

    void SetNextDailyQuestReset(uint64 time)
    {
        CharacterDatabase.PExecute("UPDATE `saved_variables` SET `NextDailyQuestResetTime` = '" UI64FMTD "'", time);
    }

    void SetNextWeeklyQuestReset(uint64 time)
    {
        CharacterDatabase.PExecute("UPDATE `saved_variables` SET `NextWeeklyQuestResetTime` = '" UI64FMTD "'", time);
    }

    void SetNextCurrencyReset(uint64 time)
    {
        CharacterDatabase.PExecute("UPDATE saved_variables SET `NextCurrenciesResetTime` = '" UI64FMTD "'", time);
    }
}
