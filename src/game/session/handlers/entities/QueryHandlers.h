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

#ifndef MANGOS_H_QUERYHANDLERS
#define MANGOS_H_QUERYHANDLERS

class WorldPacket;
class WorldSession;

/// The client's queries: a player's name, the server time, a creature's or a game object's
/// template, the player's corpse, an NPC text, an item's pages, a corpse's transport position, the
/// quests the player completed and the points of interest of the quests in its log. Static entry
/// points the opcode table binds, each borrowing the session for one call and storing nothing.
struct QueryHandlers
{
    public:
        static void HandleNameQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleQueryTime(WorldSession& session, WorldPacket& recv_data);
        static void HandleCreatureQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleGameObjectQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleCorpseQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleNpcTextQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandlePageTextQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleCorpseMapPositionQuery(WorldSession& session, WorldPacket& recv_data);
        static void HandleQueryQuestsCompleted(WorldSession& session, WorldPacket& recv_data);
        static void HandleQuestPOIQuery(WorldSession& session, WorldPacket& recv_data);
};

#endif
