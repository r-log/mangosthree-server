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

#include "session/handlers/social/TicketHandlers.h"

#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include <string>
#include "data/Language.h"
#include "WorldPacket.h"
#include "session/WorldSession.h"
#include "session/OpcodeTable.h"
#include "Log/Log.h"
#include "Object/GMTicketMgr.h"
#include "entities/player/PlayerRegistry.h"
#include "entities/player/Player.h"
#include "session/packets/chat/SystemMessage.h"
#include "persistence/characters/social/TicketRepository.h"

/**
 * @brief Handle ticket status request (CMSG_GMTICKET_GETTICKET)
 * @param recv_data World packet (empty)
 *
 * Player requests current ticket status. Responds with either:
 * - Status 0x06 + ticket data if player has active ticket
 * - Status 0x0A if no ticket exists
 *
 * Also sends server time via SendQueryTimeResponse().
 */
void TicketHandlers::HandleGMTicketGetTicket(WorldSession& session, WorldPacket& /*recv_data*/)
{
    session.SendQueryTimeResponse();

    GMTicket* ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());
    if (ticket)
    {
        session.SendGMTicketGetTicket(0x06, ticket);
    }
    else
    {
        session.SendGMTicketGetTicket(0x0A);
    }
}

/**
 * @brief Handle ticket text update (CMSG_GMTICKET_UPDATETEXT)
 * @param recv_data World packet containing new ticket text
 *
 * Updates the text of an existing ticket. Performs text cleanup:
 * - Removes invisible characters (e.g., '\a' added by client)
 * - Trims leading whitespace
 *
 * Notifies all online GMs of the update.
 */
void TicketHandlers::HandleGMTicketUpdateText(WorldSession& session, WorldPacket& recv_data)
{
    std::string ticketText;
    recv_data >> ticketText;

    // When updating the ticket, the client adds a leading '\a' char - remove it
    stripLineInvisibleChars(ticketText);

    // Trim leading spaces that may result from invisible char removal
    ltrim(ticketText);

    GMTicketResponse responce = GMTICKET_RESPONSE_UPDATE_SUCCESS;
    if (GMTicket* ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid()))
    {
        ticket->SetText(ticketText.c_str());
    }
    else
    {
        sLog.outError("Ticket update: Player %s (GUID: %u) doesn't have active ticket", session.GetPlayer()->GetName(), session.GetPlayer()->GetGUIDLow());
        responce = GMTICKET_RESPONSE_UPDATE_ERROR;
    }

    WorldPacket data(SMSG_GMTICKET_UPDATETEXT, 4);
    data << uint32(responce);
    session.SendPacket(&data);

    GMTicket * ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());

    // Notify all GM that the ticket has been changed
    sPlayerRegistry.ForEach([ticket, &session](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                SendSystemMessageFormat(*SessionOf(*player), LANG_COMMAND_TICKETUPDATED, session.GetPlayer()->GetName(), ticket->GetId());

            }
        }
    );
}

/**
 * @brief Handle ticket deletion by player (CMSG_GMTICKET_DELETETICKET)
 * @param recv_data World packet (empty)
 *
 * Player requests to close/delete their ticket. If ticket exists:
 * 1. Marks it as closed by client
 * 2. Removes from ticket manager
 * 3. Sends confirmation to client
 * 4. Updates status to show no ticket
 */
void TicketHandlers::HandleGMTicketDeleteTicket(WorldSession& session, WorldPacket& /*recv_data*/)
{
    // Mark ticket as closed if it exists
    GMTicket *ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());
    if (ticket)
    {
        ticket->CloseByClient();
    }
    sTicketMgr.Delete(session.GetPlayer()->GetObjectGuid());

    WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
    data << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
    session.SendPacket(&data);

    session.SendGMTicketGetTicket(0x0A);
}

/**
 * @brief Handle new ticket creation (CMSG_GMTICKET_CREATE)
 * @param recv_data World packet containing ticket details
 *
 * Creates a new GM ticket with:
 * - Category (harassment, bug report, etc.)
 * - Player location (map, coordinates)
 * - Ticket message text
 * - Optional chat log data (for harassment reports)
 *
 * Fails if player already has an open ticket.
 * Notifies all online GMs of the new ticket.
 */
void TicketHandlers::HandleGMTicketCreate(WorldSession& session, WorldPacket& recv_data)
{
    uint32 mapId;
    float x, y, z;
    std::string ticketText = "";
    uint8 isFollowup;
    recv_data >> mapId >> x >> y >> z;                        // last check 2.4.3
    recv_data >> ticketText;

    recv_data.read_skip<uint32>();                          // unk1, 11 - talk to gm, 1 - report problem
    recv_data >> isFollowup;                                // unk2, 1 - followup ticket
    recv_data.read_skip<uint32>();                          // unk3, 0
    recv_data.read_skip<uint32>();                          // unk4, 0

    DEBUG_LOG("TicketCreate: map %u, x %f, y %f, z %f, text %s", mapId, x, y, z, ticketText.c_str());

    if (sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid()) && !isFollowup)
    {
        WorldPacket data(SMSG_GMTICKET_CREATE, 4);
        data << uint32(GMTICKET_RESPONSE_ALREADY_EXIST);    // 1 - You already have GM ticket
        session.SendPacket(&data);
        return;
    }

    sTicketMgr.Create(session.GetPlayer()->GetObjectGuid(), ticketText.c_str());

    session.SendQueryTimeResponse();

    WorldPacket data(SMSG_GMTICKET_CREATE, 4);
    data << uint32(GMTICKET_RESPONSE_CREATE_SUCCESS);       // 2 - nothing appears (3-error creating, 5-error updating)
    session.SendPacket(&data);

    GMTicket * ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());

    sPlayerRegistry.ForEach([ticket, &session](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                SendSystemMessageFormat(*SessionOf(*player), LANG_COMMAND_TICKETNEW, session.GetPlayer()->GetName(), ticket->GetId());
            }
        }
    );
}

/**
 * @brief Handle ticket system status request (CMSG_GMTICKET_SYSTEMSTATUS)
 * @param recv_data World packet (empty)
 *
 * Player queries whether the GM ticket system is currently enabled.
 * Controlled by GM command .ticket system_on/off via sTicketMgr.
 *
 * Response: 1 = System enabled, 0 = System disabled
 */
void TicketHandlers::HandleGMTicketSystemStatus(WorldSession& session, WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);
    // Controlled by GM command .ticket system_on/off
    data << uint32(sTicketMgr.WillAcceptTickets() ? 1 : 0);
    session.SendPacket(&data);
}

namespace
{
    /// Reads the survey the client submits: the survey, up to ten answers each with its comment, and
    /// the closing comment.
    GMSurveyAnswers ReadSurvey(WorldPacket& recvData)
    {
        GMSurveyAnswers survey;
        recvData >> survey.surveyId;                           // GMSurveyCurrentSurvey.dbc
        DEBUG_LOG("SURVEY: surveyId = %u", survey.surveyId);

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

            survey.answers[i] = value;
            DEBUG_LOG("SURVEY: ID %u, value %u, text %s", questionID, value, unk_text.c_str());
        }

        recvData >> survey.comment;                            // additional comment
        DEBUG_LOG("SURVEY: comment %s", survey.comment.c_str());
        return survey;
    }
}

/**
 * @brief Handle ticket survey submission (CMSG_GMSURVEY_SUBMIT)
 * @param recv_data World packet containing survey responses
 *
 * Player submits satisfaction survey after ticket resolution.
 * Sent in response to SMSG_GM_TICKET_STATUS_UPDATE with status = 3.
 *
 * Survey data is saved to the ticket for GM/admin review.
 */
void TicketHandlers::HandleGMTicketSurveySubmit(WorldSession& session, WorldPacket& recv_data)
{
    // Sent after SMSG_GM_TICKET_STATUS_UPDATE with status = 3 (survey available)
    GMTicket* ticket = sTicketMgr.GetGMTicket(session.GetPlayer()->GetObjectGuid());
    if (!ticket)
    {
        session.SendGMTicketStatusUpdate(GM_TICKET_STATUS_CLOSE);
        // with no ticket to survey, the close above is the whole answer
        return;
    }

    ticket->SaveSurveyData(ReadSurvey(recv_data));

    // Delete the ticket now that the survey is done
    sTicketMgr.Delete(session.GetPlayer()->GetObjectGuid());

    // Inform the client the ticket is closed
    session.SendGMTicketStatusUpdate(GM_TICKET_STATUS_CLOSE);
}

void TicketHandlers::HandleGMResponseResolve(WorldSession& session, WorldPacket& recv_data)
{
    // empty opcode
    DEBUG_LOG("WORLD: %s", LookupOpcodeName(recv_data.GetOpcode()));

    sTicketMgr.Delete(session.GetPlayer()->GetObjectGuid());

    WorldPacket data(SMSG_GMTICKET_RESOLVE_RESPONSE, 1);
    data << uint8(0);                                       // ask to fill out gm survey = 1
    session.SendPacket(&data);
}

/**
 * @brief Stores a bug report or suggestion from the client.
 *
 * @param recv_data The received opcode packet.
 */
void TicketHandlers::HandleBug(WorldSession& /*session*/, WorldPacket& recv_data)
{
    uint32 suggestion, contentlen, typelen;
    std::string content, type;

    recv_data >> suggestion >> contentlen >> content;

    recv_data >> typelen >> type;

    if (suggestion == 0)
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Bug Report]");
    }
    else
    {
        DEBUG_LOG("WORLD: Received opcode CMSG_BUG [Suggestion]");
    }

    DEBUG_LOG("%s", type.c_str());
    DEBUG_LOG("%s", content.c_str());

    // Both strings are bound: any player may send this packet.
    TicketRepository::InsertBugReport(type, content);
}

void TicketHandlers::HandleComplain(WorldSession& session, WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_COMPLAIN");
    recv_data.hexlike();

    uint8 spam_type;                                        // 0 - mail, 1 - chat
    ObjectGuid spammerGuid;
    uint32 unk1 = 0;
    uint32 unk2 = 0;
    uint32 unk3 = 0;
    uint32 unk4 = 0;
    std::string description = "";
    recv_data >> spam_type;                                 // unk 0x01 const, may be spam type (mail/chat)
    recv_data >> spammerGuid;                               // player guid
    switch (spam_type)
    {
        case 0:
            recv_data >> unk1;                              // const 0
            recv_data >> unk2;                              // probably mail id
            recv_data >> unk3;                              // const 0
            break;
        case 1:
            recv_data >> unk1;                              // probably language
            recv_data >> unk2;                              // message type?
            recv_data >> unk3;                              // probably channel id
            recv_data >> unk4;                              // unk random value
            recv_data >> description;                       // spam description string (messagetype, channel name, player name, message)
            break;
    }

    // NOTE: all chat messages from this spammer automatically ignored by spam reporter until logout in case chat spam.
    // if it's mail spam - ALL mails from this spammer automatically removed by client

    // Complaint Received message
    WorldPacket data(SMSG_COMPLAIN_RESULT, 1);
    data << uint8(0);
    session.SendPacket(&data);

    DEBUG_LOG("REPORT SPAM: type %u, spammer %s, unk1 %u, unk2 %u, unk3 %u, unk4 %u, message %s", spam_type, spammerGuid.GetString().c_str(), unk1, unk2, unk3, unk4, description.c_str());
}
