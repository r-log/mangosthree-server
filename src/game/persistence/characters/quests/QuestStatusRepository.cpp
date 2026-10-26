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

#include "QuestStatusRepository.h"
#include "Database/DatabaseEnv.h"

namespace QuestStatusRepository
{
    QuestStatusRow ReadRow(Field* fields)
    {
        QuestStatusRow row;
        row.quest = fields[0].GetUInt32();
        row.status = fields[1].GetUInt32();
        row.rewarded = fields[2].GetUInt8();
        row.explored = fields[3].GetUInt8();
        row.timer = fields[4].GetUInt64();
        for (int k = 0; k < QUEST_STATUS_ROW_OBJECTIVES; ++k)
        {
            row.creatureOrGoCount[k] = fields[5 + k].GetUInt32();
        }
        for (int k = 0; k < QUEST_STATUS_ROW_ITEM_OBJECTIVES; ++k)
        {
            row.itemCount[k] = fields[9 + k].GetUInt32();
        }
        return row;
    }

    std::vector<uint32> ReadQuestIds(QueryResult* result)
    {
        std::vector<uint32> quests;
        if (result)
        {
            do
            {
                Field* fields = result->Fetch();
                quests.push_back(fields[0].GetUInt32());
            }
            while (result->NextRow());

            delete result;
        }
        return quests;
    }

    void Insert(uint32 ownerGuidLow, QuestStatusSaveRow const& row)
    {
        static SqlStatementID insertQuestStatus ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(insertQuestStatus, "INSERT INTO `character_queststatus` (`guid`,`quest`,`status`,`rewarded`,`explored`,`timer`,`mobcount1`,`mobcount2`,`mobcount3`,`mobcount4`,`itemcount1`,`itemcount2`,`itemcount3`,`itemcount4`,`itemcount5`,`itemcount6`) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.addUInt32(ownerGuidLow);
        stmt.addUInt32(row.quest);
        stmt.addUInt8(row.status);
        stmt.addUInt8(row.rewarded);
        stmt.addUInt8(row.explored);
        stmt.addUInt64(row.timer);
        for (int k = 0; k < QUEST_STATUS_ROW_OBJECTIVES; ++k)
        {
            stmt.addUInt32(row.creatureOrGoCount[k]);
        }
        for (int k = 0; k < QUEST_STATUS_ROW_ITEM_OBJECTIVES; ++k)
        {
            stmt.addUInt32(row.itemCount[k]);
        }
        stmt.Execute();
    }

    void Update(uint32 ownerGuidLow, QuestStatusSaveRow const& row)
    {
        static SqlStatementID updateQuestStatus ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updateQuestStatus, "UPDATE `character_queststatus` SET `status` = ?,`rewarded` = ?,`explored` = ?,`timer` = ?,"
                            "`mobcount1` = ?,`mobcount2` = ?,`mobcount3` = ?,`mobcount4` = ?,`itemcount1` = ?,`itemcount2` = ?,`itemcount3` = ?,`itemcount4` = ?,`itemcount5` = ?,`itemcount6` = ? WHERE `guid` = ? AND `quest` = ?");

        stmt.addUInt8(row.status);
        stmt.addUInt8(row.rewarded);
        stmt.addUInt8(row.explored);
        stmt.addUInt64(row.timer);
        for (int k = 0; k < QUEST_STATUS_ROW_OBJECTIVES; ++k)
        {
            stmt.addUInt32(row.creatureOrGoCount[k]);
        }
        for (int k = 0; k < QUEST_STATUS_ROW_ITEM_OBJECTIVES; ++k)
        {
            stmt.addUInt32(row.itemCount[k]);
        }
        stmt.addUInt32(ownerGuidLow);
        stmt.addUInt32(row.quest);
        stmt.Execute();
    }

    void SaveWeekly(uint32 ownerGuidLow, std::set<uint32> const& quests)
    {
        static SqlStatementID delQuestStatus ;
        static SqlStatementID insQuestStatus  ;

        SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus, "DELETE FROM `character_queststatus_weekly` WHERE `guid` = ?");
        SqlStatement stmtIns =  CharacterDatabase.CreateStatement(insQuestStatus, "INSERT INTO `character_queststatus_weekly` (`guid`,`quest`) VALUES (?, ?)");

        stmtDel.PExecute(ownerGuidLow);

        for (std::set<uint32>::const_iterator iter = quests.begin(); iter != quests.end(); ++iter)
        {
            uint32 quest_id  = *iter;
            stmtIns.PExecute(ownerGuidLow, quest_id);
        }
    }

    void SaveMonthly(uint32 ownerGuidLow, std::set<uint32> const& quests)
    {
        static SqlStatementID deleteQuest ;
        static SqlStatementID insertQuest ;

        SqlStatement stmtDel = CharacterDatabase.CreateStatement(deleteQuest, "DELETE FROM `character_queststatus_monthly` WHERE `guid` = ?");
        SqlStatement stmtIns = CharacterDatabase.CreateStatement(insertQuest, "INSERT INTO `character_queststatus_monthly` (`guid`, `quest`) VALUES (?, ?)");

        stmtDel.PExecute(ownerGuidLow);

        for (std::set<uint32>::const_iterator iter = quests.begin(); iter != quests.end(); ++iter)
        {
            uint32 quest_id = *iter;
            stmtIns.PExecute(ownerGuidLow, quest_id);
        }
    }

    void SaveDaily(uint32 ownerGuidLow, std::vector<uint32> const& quests)
    {
        static SqlStatementID delQuestStatus ;
        static SqlStatementID insQuestStatus ;

        SqlStatement stmtDel = CharacterDatabase.CreateStatement(delQuestStatus, "DELETE FROM `character_queststatus_daily` WHERE `guid` = ?");
        SqlStatement stmtIns = CharacterDatabase.CreateStatement(insQuestStatus, "INSERT INTO `character_queststatus_daily` (`guid`,`quest`) VALUES (?, ?)");

        stmtDel.PExecute(ownerGuidLow);

        for (std::vector<uint32>::const_iterator iter = quests.begin(); iter != quests.end(); ++iter)
        {
            stmtIns.PExecute(ownerGuidLow, *iter);
        }
    }

    void DeleteEveryDaily()
    {
        CharacterDatabase.Execute("DELETE FROM `character_queststatus_daily`");
    }

    void DeleteEveryWeekly()
    {
        CharacterDatabase.Execute("DELETE FROM `character_queststatus_weekly`");
    }

    void TruncateMonthly()
    {
        CharacterDatabase.Execute("TRUNCATE character_queststatus_monthly");
    }
}
