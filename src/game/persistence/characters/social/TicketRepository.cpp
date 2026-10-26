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

#include "TicketRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace TicketRepository
{
    uint32 ReadHighestTicketId()
    {
        uint32 highestRow = 0;
        if (QueryResult* highest = CharacterDatabase.Query("SELECT MAX(`ticket_id`) FROM `character_ticket`"))
        {
            highestRow = (*highest)[0].GetUInt32();
            delete highest;
        }
        return highestRow;
    }

    uint32 ReadAutoIncrementNext()
    {
        uint32 autoIncrementNext = 0;
        if (QueryResult* next = CharacterDatabase.Query("SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'"))
        {
            autoIncrementNext = (*next)[0].GetUInt32();         // NULL (no counter) reads as 0
            delete next;
        }
        return autoIncrementNext;
    }

    static GMTicketRow ReadTicket(Field* fields)
    {
        GMTicketRow row;
        row.guid         = fields[0].GetUInt32();
        row.text         = fields[1].GetCppString();
        row.responseText = fields[2].GetCppString();
        row.lastChange   = fields[3].GetUInt64();
        row.ticketId     = fields[4].GetUInt32();
        return row;
    }

    std::vector<GMTicketRow> QueryOpenTickets()
    {
        QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC");
        return ReadRows<GMTicketRow>(result, ReadTicket);
    }

    void Insert(uint32 ticketId, uint32 guid, char const* text)
    {
        static SqlStatementID insTicket;
        SqlStatement insert = CharacterDatabase.CreateStatement(insTicket,
                              "INSERT INTO `character_ticket` (`ticket_id`, `guid`, `ticket_text`) VALUES (?, ?, ?)");
        insert.addUInt32(ticketId);
        insert.addUInt32(guid);
        insert.addString(text);
        insert.Execute();
    }

    void SetText(uint32 guid, uint32 ticketId, std::string const& text)
    {
        static SqlStatementID updTicketText;
        SqlStatement update = CharacterDatabase.CreateStatement(updTicketText,
                              "UPDATE `character_ticket` SET `ticket_text` = ? "
                              "WHERE `guid` = ? AND `ticket_id` = ?");
        update.addString(text);
        update.addUInt32(guid);
        update.addUInt32(ticketId);
        update.Execute();
    }

    void SetResponseText(uint32 guid, uint32 ticketId, std::string const& text)
    {
        static SqlStatementID updTicketResponse;
        SqlStatement update = CharacterDatabase.CreateStatement(updTicketResponse,
                              "UPDATE `character_ticket` SET `response_text` = ? "
                              "WHERE `guid` = ? and `ticket_id` = ?");
        update.addString(text);
        update.addUInt32(guid);
        update.addUInt32(ticketId);
        update.Execute();
    }

    void Resolve(uint32 guid)
    {
        CharacterDatabase.PExecute("UPDATE `character_ticket` "
                                   "SET `resolved` = 1 "
                                   "WHERE `guid` = %u AND `resolved` = 0",
                                   guid);
    }

    void DeleteAll()
    {
        CharacterDatabase.Execute("DELETE FROM `character_ticket`");
    }

    void DeleteOpenTickets(uint32 guid)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_ticket` "
                                   "WHERE `resolved` = 0 AND `guid` = %u",
                                   guid);
    }

    void InsertSurvey(GMSurveyRow const& row)
    {
        static SqlStatementID insGmSurvey;
        SqlStatement insert = CharacterDatabase.CreateStatement(insGmSurvey,
                              "INSERT INTO `gm_surveys` (`guid`, `surveyid`, `main_survey`, "
                              "`answer1`, `answer2`, `answer3`, `answer4`, `answer5`, "
                              "`answer6`, `answer7`, `answer8`, `answer9`, `answer10`, `comment`) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insert.addUInt32(row.guid);
        insert.addUInt32(row.ticketId);
        insert.addUInt32(row.mainSurvey);
        for (int i = 0; i < GM_SURVEY_ROW_ANSWERS; ++i)
        {
            insert.addUInt32(row.answers[i]);
        }
        insert.addString(row.comment);
        insert.Execute();
    }

    void InsertBugReport(std::string const& type, std::string const& content)
    {
        static SqlStatementID insBugReport;
        SqlStatement insert = CharacterDatabase.CreateStatement(insBugReport,
                              "INSERT INTO `bugreport` (`type`,`content`) VALUES(?, ?)");
        insert.addString(type);
        insert.addString(content);
        insert.Execute();
    }

    void InsertWhisper(uint32 toGuid, uint32 fromGuid, std::string const& message, uint32 ticketId)
    {
        static SqlStatementID wlog;
        SqlStatement stmt = CharacterDatabase.CreateStatement(wlog, "INSERT INTO `character_whispers` (`to_guid`, `from_guid`, `message`, `regarding_ticket_id`) VALUES (?, ?, ?, ?)");
        stmt.addUInt32(toGuid);
        stmt.addUInt32(fromGuid);
        stmt.addString(message);
        stmt.addUInt32(ticketId);
        stmt.Execute();
    }
}
