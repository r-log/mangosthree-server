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
#include "Tools/Language.h"
#include "WorldPacket.h"
#include "Server/WorldSession.h"
#include "Server/OpcodeTable.h"
#include "Log/Log.h"
#include "Object/GMTicketMgr.h"
#include "entities/player/PlayerRegistry.h"
#include "entities/player/Player.h"
#include "WorldHandlers/Chat.h"

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
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETUPDATED, session.GetPlayer()->GetName(), ticket->GetId());

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
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, session.GetPlayer()->GetName(), ticket->GetId());
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
        //Should we send GM_TICKET_STATUS_CLOSE here aswell?
        return;
    }

    ticket->SaveSurveyData(recv_data);

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
