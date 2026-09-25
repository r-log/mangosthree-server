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

#include "Platform/Define.h"
#include <algorithm>
#include <cstring>
#include <ctime>
#include <string>
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "GMTicketMgr.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Policies/Singleton.h"
#include "Player.h"


/**
 * @brief Stores GM survey answers from a received packet.
 *
 * @param recvData The packet containing survey responses and optional comments.
 */
void GMTicket::SaveSurveyData(WorldPacket& recvData) const
{
    uint32 surveyId;
    recvData >> surveyId;                                  // GMSurveyCurrentSurvey.dbc
    DEBUG_LOG("SURVEY: surveyId = %u", surveyId);

    uint8 result[10];
    memset(result, 0, sizeof(result));
    for (int i = 0; i < 10; ++i)
    {
        uint32 questionID;
        recvData >> questionID;                            // GMSurveyQuestions.dbc
        if (!questionID)
        {
            break;
        }

        uint8 value;
        std::string unk_text;
        recvData >> value;                                 // answer
        recvData >> unk_text;                              // comment per question

        result[i] = value;
        DEBUG_LOG("SURVEY: ID %u, value %u, text %s", questionID, value, unk_text.c_str());
    }

    std::string comment;
    recvData >> comment;                                   // additional comment
    DEBUG_LOG("SURVEY: comment %s", comment.c_str());

    // Save survey data to database. Decoupling D7i: the comment is a bound parameter (C5),
    // on a path any player walks -- CMSG_GMSURVEY_SUBMIT after a ticket is closed.
    static SqlStatementID insGmSurvey;
    SqlStatement insert = CharacterDatabase.CreateStatement(insGmSurvey,
                          "INSERT INTO `gm_surveys` (`guid`, `surveyid`, `main_survey`, "
                          "`answer1`, `answer2`, `answer3`, `answer4`, `answer5`, "
                          "`answer6`, `answer7`, `answer8`, `answer9`, `answer10`, `comment`) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.addUInt32(m_guid.GetCounter());
    insert.addUInt32(m_ticketId);
    insert.addUInt32(surveyId);
    for (int i = 0; i < 10; ++i)
    {
        insert.addUInt32(result[i]);
    }
    insert.addString(comment);
    insert.Execute();
}

/**
 * @brief Initializes ticket data from loaded or newly created values.
 *
 * @param guid The player GUID owning the ticket.
 * @param text The ticket text.
 * @param responseText The GM response text.
 * @param update The last update time.
 * @param ticketId The ticket identifier.
 */
void GMTicket::Init(ObjectGuid guid, const std::string& text, const std::string& responseText, time_t update, uint32 ticketId)
{
    m_guid = guid;
    m_ticketId = ticketId;
    m_text = text;
    m_responseText = responseText;
    m_lastUpdate = update;
}

/**
 * @brief Updates the ticket text and persists it to the database.
 *
 * @param text The new ticket text.
 */
void GMTicket::SetText(const char* text)
{
    m_text = text ? text : "";
    m_lastUpdate = time(NULL);

    // Decoupling D7i: bound parameter (C5). CMSG_GMTICKET_UPDATETEXT, a player's own edit.
    static SqlStatementID updTicketText;
    SqlStatement update = CharacterDatabase.CreateStatement(updTicketText,
                          "UPDATE `character_ticket` SET `ticket_text` = ? "
                          "WHERE `guid` = ? AND `ticket_id` = ?");
    update.addString(m_text);
    update.addUInt32(m_guid.GetCounter());
    update.addUInt32(m_ticketId);
    update.Execute();
}

/**
 * @brief Updates the GM response text and persists it to the database.
 *
 * @param text The new response text.
 */
void GMTicket::SetResponseText(const char* text)
{
    m_responseText = text ? text : "";

    // Perform action in DB only if text is not empty
    if (m_responseText != "")
    {
        m_lastUpdate = time(NULL);

        // Decoupling D7i. This one is GM-only (`.ticket respond`), so ExecuteCommand's
        // widened AdminScope already covers it -- it is converted anyway, because it is
        // the same three lines as GMTicket::SetText above and leaving one escape behind in
        // a converted file would cost the gate an allow line for nothing.
        static SqlStatementID updTicketResponse;
        SqlStatement update = CharacterDatabase.CreateStatement(updTicketResponse,
                              "UPDATE `character_ticket` SET `response_text` = ? "
                              "WHERE `guid` = ? and `ticket_id` = ?");
        update.addString(m_responseText);
        update.addUInt32(m_guid.GetCounter());
        update.addUInt32(m_ticketId);
        update.Execute();
    }
}

/**
 * @brief Closes the ticket and requests a survey from the client.
 */
void GMTicket::CloseWithSurvey() const
{
    _Close(GM_TICKET_STATUS_SURVEY);
}

/**
 * @brief Closes the ticket from the client side without further action.
 */
void GMTicket::CloseByClient() const
{
    _Close(GM_TICKET_STATUS_DO_NOTHING);
}

/**
 * @brief Closes the ticket with the standard close status.
 */
void GMTicket::Close() const
{
    _Close(GM_TICKET_STATUS_CLOSE);
}

/**
 * @brief Marks the ticket resolved and optionally notifies the player.
 *
 * @param statusCode The client status code to send on closure.
 */
void GMTicket::_Close(GMTicketStatus statusCode) const
{
    Player* pPlayer = sObjectMgr.GetPlayer(m_guid);

    CharacterDatabase.PExecute("UPDATE `character_ticket` "
                               "SET `resolved` = 1 "
                               "WHERE `guid` = %u AND `resolved` = 0",
                               m_guid.GetCounter());

    if (pPlayer && statusCode != GM_TICKET_STATUS_DO_NOTHING)
    {
        pPlayer->GetSession()->SendGMTicketStatusUpdate(statusCode);
    }
}

/**
 * @brief Loads unresolved GM tickets from the database.
 */
void GMTicketMgr::LoadGMTickets()
{
    m_GMTicketMap.clear();                                  // For reload case

    // Decoupling D7i: the id GMTicketMgr::Create hands the next ticket. Read here, at
    // start-up, instead of by the synchronous SELECT-after-INSERT Create used to run on
    // the tick. Over the WHOLE table, because the loader below reads only the unresolved
    // rows and a resolved row's id is still taken.
    //
    // Fix round 1: and never below the table's own AUTO_INCREMENT. The column is still
    // AUTO_INCREMENT, and InnoDB (MariaDB 10.4) persists that counter across a restart
    // only as MAX + 1 recomputed at open -- but a row that was inserted and then deleted
    // while the server ran has already moved it past MAX. Seeding from
    // max(MAX(ticket_id), AUTO_INCREMENT - 1) is what AUTO_INCREMENT itself would have
    // handed out next, so ids stay identical to the column's within a run and across one.
    uint32 highestRow = 0;
    if (QueryResult* highest = CharacterDatabase.Query("SELECT MAX(`ticket_id`) FROM `character_ticket`"))
    {
        highestRow = (*highest)[0].GetUInt32();
        delete highest;
    }

    uint32 autoIncrementNext = 0;
    if (QueryResult* next = CharacterDatabase.Query("SELECT `AUTO_INCREMENT` FROM `information_schema`.`TABLES` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'character_ticket'"))
    {
        autoIncrementNext = (*next)[0].GetUInt32();         // NULL (no counter) reads as 0
        delete next;
    }

    m_highestTicketId = std::max(highestRow, autoIncrementNext ? autoIncrementNext - 1 : 0u);

    //                                                 0       1              2                3                                    4
    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `ticket_text`, `response_text`, UNIX_TIMESTAMP(`ticket_lastchange`), `ticket_id` FROM `character_ticket` WHERE `resolved` = 0 ORDER BY `ticket_id` ASC");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `character_ticket`, table is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 guidlow = fields[0].GetUInt32();
        if (!guidlow)
        {
            continue;
        }

        ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlow);
        GMTicket& ticket = m_GMTicketMap[guid];

        ticket.Init(guid, fields[1].GetCppString(), fields[2].GetCppString(), time_t(fields[3].GetUInt64()), fields[4].GetUInt32());
        m_GMTicketIdMap[ticket.GetId()] = &ticket;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %zu GM tickets", GetTicketCount());
    sLog.outString();
}

/**
 * @brief Creates a new GM ticket for a player.
 *
 * @param guid The GUID of the player creating the ticket.
 * @param text The ticket text.
 */
void GMTicketMgr::Create(ObjectGuid guid, const char* text)
{
    // Decoupling D7i. This was the worst shape in the player-reachable residual: a
    // synchronous INSERT ("This needs to be Direct ... as we need the id of it soon
    // afterwards") followed by a synchronous SELECT to read back the id AUTO_INCREMENT had
    // just chosen -- both on the world thread, inside World::Update, on a packet any
    // player may send.
    //
    // The id is chosen here instead, from the counter LoadGMTickets primed at start-up,
    // and written WITH the row, so one queued INSERT does the whole job. The value is the
    // same one the read used to come back with: the old SELECT ordered by `ticket_id` DESC
    // and took the first, which is the row the INSERT had just added, and AUTO_INCREMENT
    // hands out MAX + 1 exactly as this counter does. The escape is a bound parameter (C5).
    const uint32 ticketId = ++m_highestTicketId;

    static SqlStatementID insTicket;
    SqlStatement insert = CharacterDatabase.CreateStatement(insTicket,
                          "INSERT INTO `character_ticket` (`ticket_id`, `guid`, `ticket_text`) VALUES (?, ?, ?)");
    insert.addUInt32(ticketId);
    insert.addUInt32(guid.GetCounter());
    insert.addString(text);
    insert.Execute();

    //This implicitly creates a new instance since we're using operator[]
    GMTicket& ticket = m_GMTicketMap[guid];
    if (ticket.GetPlayerGuid())
    {
        m_GMTicketIdMap.erase(ticketId);
    }

    //Lets reinitialize with new data
    ticket.Init(guid, text, "", time(NULL), ticketId);
    m_GMTicketIdMap[ticketId] = &ticket;
}

/**
 * @brief Deletes all GM tickets and notifies affected online players.
 */
void GMTicketMgr::DeleteAll()
{
    for (GMTicketMap::const_iterator itr = m_GMTicketMap.begin(); itr != m_GMTicketMap.end(); ++itr)
    {
        if (Player* owner = sObjectMgr.GetPlayer(itr->first))
        {
            owner->GetSession()->SendGMTicketGetTicket(0x0A);
        }
    }
    CharacterDatabase.Execute("DELETE FROM `character_ticket`");
    m_GMTicketIdMap.clear();
    m_GMTicketMap.clear();
}
