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

#ifndef MANGOS_H_MAILHANDLERS
#define MANGOS_H_MAILHANDLERS

#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SessionId.h"

class WorldPacket;
class WorldSession;

/// The client's mail opcodes: a mail sent, marked as read, deleted, returned, its item or its money
/// taken, the mail list, a mail copied into a letter item and the next mail time. A mail to an
/// offline receiver is sent a tick later, once the receiver's mailbox count is read. Static entry
/// points the opcode table binds, each borrowing the session for one call and storing nothing.
struct MailHandlers
{
    public:
        /// Everything CMSG_SEND_MAIL carries, captured by value into the continuation,
        /// plus the receiver guid the handler resolved from memory.
        struct MailSendRequest
        {
            ObjectGuid mailboxGuid;
            ObjectGuid receiverGuid;
            std::vector<ObjectGuid> itemGuids;
            uint64 money = 0;
            uint64 COD = 0;
            std::string receiver;
            std::string subject;
            std::string body;
            uint32 unk1 = 0;
            uint32 unk2 = 0;
        };

        static void HandleSendMail(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailMarkAsRead(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailDelete(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailReturnToSender(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailTakeItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailTakeMoney(WorldSession& session, WorldPacket& recv_data);
        static void HandleGetMailList(WorldSession& session, WorldPacket& recv_data);
        static void HandleMailCreateTextItem(WorldSession& session, WorldPacket& recv_data);
        static void HandleQueryNextMailTime(WorldSession& session, WorldPacket& recv_data);
        static void QueueSendMailboxCountRead(uint32 accountId, proto::SessionId sessionId,
                                              ObjectGuid playerGuid, MailSendRequest request);

    private:
        static bool CheckMailBox(WorldSession& session, ObjectGuid guid);
        static void HandleSendMailCallback(std::optional<uint32> const& count, uint32 accountId,
                                           proto::SessionId sessionId, ObjectGuid playerGuid,
                                           MailSendRequest request);
        static void ProcessSendMail(WorldSession& session, MailSendRequest const& request, uint8 offlineMailsCount);
};

#endif
