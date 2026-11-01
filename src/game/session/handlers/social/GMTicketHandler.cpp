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

/**
 * @file GMTicketHandler.cpp
 * @brief Defines two WorldSession members: SendGMTicketGetTicket, which sends the client its GM ticket's
 * status and text, and SendGMTicketStatusUpdate, which tells the client its ticket's status changed.
 */

#include "Platform/Define.h"
#include <string>
#include "WorldPacket.h"
#include "GMTicketMgr.h"
#include "WorldSession.h"

/**
 * @brief Send ticket status to client
 * @param status Response status code:
 *               0x0A = No ticket
 *               0x06 = Ticket exists (includes ticket data)
 * @param ticket Active ticket pointer (required if status == 6)
 *
 * Constructs and sends SMSG_GMTICKET_GETTICKET with ticket information.
 * When status is 0x06, includes ticket text and queue position info.
 */
void WorldSession::SendGMTicketGetTicket(uint32 status, GMTicket* ticket /*= NULL*/)
{
    std::string text = ticket ? ticket->GetText() : "";

    int len = text.size() + 1;
    WorldPacket data(SMSG_GMTICKET_GETTICKET, (4 + len + 1 + 4 + 2 + 4 + 4));
    data << uint32(status);                                 // standard 0x0A, 0x06 if text present
    if (status == 6)
    {
        data << uint32(123);                                // unk
        data << text;                                       // ticket text
        data << uint8(0x7);                                 // ticket category
        data << float(0);                                   // tickets in queue?
        data << float(0);                                   // if > "tickets in queue" then "We are currently experiencing a high volume of petitions."
        data << float(0);                                   // 0 - "Your ticket will be serviced soon", 1 - "Wait time currently unavailable"
        data << uint8(0);                                   // if == 2 and next field == 1 then "Your ticket has been escalated"
        data << uint8(0);                                   // const
        data << uint8(0);                                   // average wait time string
        data << uint32(0);                                  // average wait time
    }
    SendPacket(&data);
}

/**
 * @brief Send ticket status update to client
 * @param statusCode Status code to send
 *
 * Sends SMSG_GM_TICKET_STATUS_UPDATE to notify the player of ticket status changes.
 * Status codes:
 * - 0 = Ticket updated
 * - 1 = Ticket closed
 * - 2 = Ticket being handled by GM
 * - 3 = Survey available (show survey dialog)
 */
void WorldSession::SendGMTicketStatusUpdate(GMTicketStatus statusCode)
{
    WorldPacket data(SMSG_GM_TICKET_STATUS_UPDATE, 4);
    data << uint32(statusCode);
    SendPacket(&data);
}
