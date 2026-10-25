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

#include "AchievementRepository.h"
#include "Database/DatabaseEnv.h"

namespace AchievementRepository
{
    QueryResult* QueryCompletedAchievements()
    {
        return CharacterDatabase.Query("SELECT `achievement` FROM `character_achievement` GROUP BY `achievement`");
    }

    QueryResult* QueryProgressCriteria()
    {
        return CharacterDatabase.Query("SELECT DISTINCT `criteria` FROM `character_achievement_progress`");
    }

    void DeleteAllOf(uint32 guid)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `character_achievement` WHERE `guid` = %u", guid);
        CharacterDatabase.PExecute("DELETE FROM `character_achievement_progress` WHERE `guid` = %u", guid);
        CharacterDatabase.CommitTransaction();
    }

    void ReplaceCompleted(uint32 guid, uint32 achievement, uint64 date)
    {
        static SqlStatementID delComplAchievements ;
        static SqlStatementID insComplAchievements ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delComplAchievements, "DELETE FROM `character_achievement` WHERE `guid` = ? AND `achievement` = ?");
        stmt.PExecute(guid, achievement);

        stmt = CharacterDatabase.CreateStatement(insComplAchievements, "INSERT INTO `character_achievement` (`guid`, `achievement`, `date`) VALUES (?, ?, ?)");
        stmt.PExecute(guid, achievement, date);
    }

    void DeleteCompleted(uint32 guid, uint32 achievement)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_achievement` WHERE `guid` = %u AND `achievement` = %u",
                                   guid, achievement);
    }

    void DeleteProgress(uint32 guid, uint32 criteria)
    {
        static SqlStatementID delProgress ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(delProgress, "DELETE FROM `character_achievement_progress` WHERE `guid` = ? AND `criteria` = ?");
        stmt.PExecute(guid, criteria);
    }

    void InsertProgress(uint32 guid, uint32 criteria, uint32 counter, uint64 date)
    {
        static SqlStatementID insProgress ;
        SqlStatement stmt = CharacterDatabase.CreateStatement(insProgress, "INSERT INTO `character_achievement_progress` (`guid`, `criteria`, `counter`, `date`) VALUES (?, ?, ?, ?)");
        stmt.PExecute(guid, criteria, counter, date);
    }

    void DeleteAchievementOfEveryCharacter(uint32 achievement)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_achievement` WHERE `achievement` = %u", achievement);
    }

    void DeleteCriteriaOfEveryCharacter(uint32 criteria)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_achievement_progress` WHERE `criteria` = %u", criteria);
    }
}
