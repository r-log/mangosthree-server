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

#ifndef MANGOS_H_QUESTHANDLERS
#define MANGOS_H_QUESTHANDLERS

class ObjectGuid;
class WorldPacket;
class WorldSession;

/// The client's quest opcodes: a quest giver's status, one at a time or for every quest giver in
/// sight, its greeting, a quest's details, accepted, its reward asked for and chosen, its items
/// handed in, a quest queried, removed from the log, confirmed when a party member shares it,
/// shared with the party and the answer to a share. Static entry points the opcode table binds,
/// each borrowing the session for one call and storing nothing.
struct QuestHandlers
{
    public:
        static void HandleQuestgiverStatusQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverHello(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverAcceptQuest(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverQueryQuest(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverChooseReward(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverRequestReward(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestLogRemoveQuest(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestConfirmAccept(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverCompleteQuest(WorldSession& session, WorldPacket& recv_data);
        static void HandlePushQuestToParty(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestPushResult(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestgiverStatusMultipleQuery(WorldSession& session, WorldPacket& recv_data);

    private:
        static bool CanInteractWithQuestGiver(WorldSession& session, ObjectGuid guid, char const* descr);
};

#endif
