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

#ifndef MANGOS_H_CONTACTHANDLERS
#define MANGOS_H_CONTACTHANDLERS

#include <string>
#include "data/dbc/SharedDefines.h"

class ObjectGuid;
class WorldPacket;
class WorldSession;

/// The client's contact list opcodes: the list request, a friend added by name with a note, a friend
/// removed, a player ignored by name or no longer ignored, and a contact's note: static entry points
/// the opcode table binds, each borrowing the session for one call and storing nothing.
/// The private statics answer an add-friend or add-ignore request once the name has been
/// resolved to a character.
struct ContactHandlers
{
    public:
        static void HandleContactList(WorldSession& session, WorldPacket& recv_data);
        static void HandleAddFriend(WorldSession& session, WorldPacket& recv_data);
        static void HandleDelFriend(WorldSession& session, WorldPacket& recv_data);
        static void HandleAddIgnore(WorldSession& session, WorldPacket& recv_data);
        static void HandleDelIgnore(WorldSession& session, WorldPacket& recv_data);
        static void HandleSetContactNotes(WorldSession& session, WorldPacket& recv_data);

    private:
        static void CompleteAddFriend(WorldSession& session, ObjectGuid friendGuid, Team team, std::string const& friendNote);
        static void CompleteAddIgnore(WorldSession& session, ObjectGuid ignoreGuid);
};

#endif
