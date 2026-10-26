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
#include "persistence/characters/social/TicketRepository.h"

static_assert(int(GM_SURVEY_ROW_ANSWERS) == 10, "the saved survey row's answers");


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

    // Save survey data to database, the comment bound.
    GMSurveyRow row;
    row.guid       = m_guid.GetCounter();
    row.ticketId   = m_ticketId;
    row.mainSurvey = surveyId;
    for (int i = 0; i < 10; ++i)
    {
        row.answers[i] = result[i];
    }
    row.comment    = comment;
    TicketRepository::InsertSurvey(row);
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

    TicketRepository::SetText(m_guid.GetCounter(), m_ticketId, m_text);
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

        TicketRepository::SetResponseText(m_guid.GetCounter(), m_ticketId, m_responseText);
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

    TicketRepository::Resolve(m_guid.GetCounter());

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

    // The id GMTicketMgr::Create hands the next ticket, read over the whole table (a resolved
    // row's id is still taken) and never below the table's own AUTO_INCREMENT less one: a row
    // inserted and deleted while the server ran has moved that counter past MAX, so this is the
    // id AUTO_INCREMENT itself would hand out next.
    uint32 highestRow = TicketRepository::ReadHighestTicketId();
    uint32 autoIncrementNext = TicketRepository::ReadAutoIncrementNext();

    m_highestTicketId = std::max(highestRow, autoIncrementNext ? autoIncrementNext - 1 : 0u);

    std::vector<GMTicketRow> tickets = TicketRepository::QueryOpenTickets();

    if (tickets.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `character_ticket`, table is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(tickets.size());

    for (GMTicketRow const& row : tickets)
    {
        bar.step();

        uint32 guidlow = row.guid;
        if (!guidlow)
        {
            continue;
        }

        ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlow);
        GMTicket& ticket = m_GMTicketMap[guid];

        ticket.Init(guid, row.text, row.responseText, time_t(row.lastChange), row.ticketId);
        m_GMTicketIdMap[ticket.GetId()] = &ticket;
    }

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
    // The id comes from the counter LoadGMTickets primed at startup and is written with the
    // row, so one queued INSERT does the whole job; it is the id AUTO_INCREMENT would choose.
    const uint32 ticketId = ++m_highestTicketId;

    TicketRepository::Insert(ticketId, guid.GetCounter(), text);

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
    TicketRepository::DeleteAll();
    m_GMTicketIdMap.clear();
    m_GMTicketMap.clear();
}
