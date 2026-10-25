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

#ifndef MANGOS_H_TICKETREPOSITORY
#define MANGOS_H_TICKETREPOSITORY

#include <string>
#include "Platform/Define.h"

class Field;
class QueryResult;

/// The number of answers a saved GM survey holds.
enum
{
    GM_SURVEY_ROW_ANSWERS = 10
};

/// A GM survey's saved row, in the order of the INSERT's columns.
struct GMSurveyRow
{
    uint32 guid = 0;
    uint32 ticketId = 0;
    uint32 mainSurvey = 0;
    uint32 answers[GM_SURVEY_ROW_ANSWERS] = {};
    std::string comment;
};

/// One row of the open ticket load: its owner, text, response, last change and id.
struct GMTicketRow
{
    uint32 guid = 0;
    std::string text;
    std::string responseText;
    uint64 lastChange = 0;
    uint32 ticketId = 0;
};

/// The GM tickets', surveys' and bug reports' saved rows (`character_ticket`, `gm_surveys`,
/// `bugreport`). The three loads run at startup, synchronously; every write is queued on the
/// character database.
namespace TicketRepository
{
    /// The highest ticket id in the table, 0 when there is none.
    uint32 ReadHighestTicketId();

    /// The table's next AUTO_INCREMENT value, 0 when there is none.
    uint32 ReadAutoIncrementNext();

    /// Selects every open ticket, ordered by id: owner, text, response, last change, id.
    QueryResult* QueryOpenTickets();

    /// Reads one row of the open ticket load.
    GMTicketRow ReadTicket(Field* fields);

    /// Inserts a new ticket's row with its id, the text bound.
    void Insert(uint32 ticketId, uint32 guid, char const* text);

    /// Sets a ticket's text, bound.
    void SetText(uint32 guid, uint32 ticketId, std::string const& text);

    /// Sets a ticket's GM response, bound.
    void SetResponseText(uint32 guid, uint32 ticketId, std::string const& text);

    /// Marks the owner's open ticket resolved.
    void Resolve(uint32 guid);

    /// Deletes every ticket.
    void DeleteAll();

    /// Deletes a character's open tickets, as the character's delete does.
    void DeleteOpenTickets(uint32 guid);

    /// Inserts a survey's row, the comment bound.
    void InsertSurvey(GMSurveyRow const& row);

    /// Inserts a bug report or a suggestion, both strings bound.
    void InsertBugReport(std::string const& type, std::string const& content);

    /// Logs a whisper, the message bound, with the ticket it is about (0 for none).
    void InsertWhisper(uint32 toGuid, uint32 fromGuid, std::string const& message, uint32 ticketId);
}

#endif
