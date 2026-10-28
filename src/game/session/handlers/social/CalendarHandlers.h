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

#ifndef MANGOS_H_CALENDARHANDLERS
#define MANGOS_H_CALENDARHANDLERS

#include <optional>
#include <string>
#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "SessionId.h"

class Player;
class WorldPacket;
class WorldSession;
struct CharacterCacheEntry;

/// The client's calendar opcodes: the calendar and an event, the guild and arena team invites,
/// a guild event signed up for, an event added, updated, removed or copied, a player invited, an
/// invite answered, removed or given a status or a moderator's rank, a complaint and the count
/// of pending invites. An invite to an offline player is finished a tick later, once the
/// invitee's ignore flag is read. Static entry points the opcode table binds, each borrowing the
/// session for one call and storing nothing.
struct CalendarHandlers
{
    public:
        /// Everything CMSG_CALENDAR_EVENT_INVITE carries, plus the invitee the handler
        /// resolved from the character cache.
        struct CalendarInviteRequest
        {
            ObjectGuid playerGuid;                          ///< the inviter
            ObjectGuid inviteeGuid;                         ///< empty = no such character
            uint32 inviteeTeam = 0;
            uint32 inviteeGuildId = 0;
            uint64 eventId = 0;
            std::string name;
            bool isPreInvite = false;
            bool isGuildEvent = false;
        };

        static void HandleCalendarGetCalendar(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarGetEvent(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarGuildFilter(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventSignup(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarArenaTeam(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarAddEvent(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarUpdateEvent(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarRemoveEvent(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarCopyEvent(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventInvite(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventRsvp(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventRemoveInvite(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventStatus(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarEventModeratorStatus(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarComplain(WorldSession& session, WorldPacket& recv_data);
        static void HandleCalendarGetNumPending(WorldSession& session, WorldPacket& recv_data);
        static void ResolveCalendarInvitee(CharacterCacheEntry const& cached, CalendarInviteRequest& request);
        static void QueueCalendarInviteIgnoreRead(uint32 accountId, proto::SessionId sessionId,
                                                  CalendarInviteRequest request);

    private:
        static void HandleCalendarInviteIgnoreCallback(std::optional<uint8> const& flags, uint32 accountId,
                                                       proto::SessionId sessionId, CalendarInviteRequest request);
        static void FinishCalendarEventInvite(Player* player, CalendarInviteRequest const& request, bool isIgnored);
};

#endif
