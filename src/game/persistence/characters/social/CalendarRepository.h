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

#ifndef MANGOS_H_CALENDARREPOSITORY
#define MANGOS_H_CALENDARREPOSITORY

#include <string>
#include <vector>
#include "Platform/Define.h"

/// A calendar event's saved row.
struct CalendarEventRow
{
    uint64 eventId = 0;
    uint32 creatorGuid = 0;
    uint32 guildId = 0;
    uint32 type = 0;
    uint32 flags = 0;
    int32 dungeonId = 0;
    uint32 eventTime = 0;
    std::string title;
    std::string description;
};

/// A calendar invite's saved row.
struct CalendarInviteRow
{
    uint64 inviteId = 0;
    uint64 eventId = 0;
    uint32 inviteeGuid = 0;
    uint32 senderGuid = 0;
    uint8 status = 0;
    uint32 lastUpdateTime = 0;
    uint8 rank = 0;
};

/// The calendar's saved rows (`calendar_events`, `calendar_invites`). The two loads and the two
/// truncates run at startup, synchronously; every other write is queued on the character database.
namespace CalendarRepository
{
    /// Selects every event, ordered by id: id, creator, guild, type, flags, dungeon, time, title,
    /// description.
    std::vector<CalendarEventRow> QueryEvents();

    /// Selects every invite, ordered by id: id, event, invitee, sender, status, last update, rank.
    std::vector<CalendarInviteRow> QueryInvites();

    /// Each empties one of the two tables, at startup.
    void TruncateEvents();
    void TruncateInvites();

    /// Inserts an event's row, the title and description bound.
    void InsertEvent(CalendarEventRow const& row);

    /// Updates an event's type, flags, dungeon, time, title and description, the strings bound.
    void UpdateEvent(CalendarEventRow const& row);

    /// Deletes an event's row.
    void DeleteEvent(uint64 eventId);

    /// Inserts an invite's row.
    void InsertInvite(uint64 inviteId, uint64 eventId, uint32 inviteeGuid, uint32 senderGuid, uint32 status, uint32 statusTime, uint32 rank);

    /// Deletes an invite's row.
    void DeleteInvite(uint64 inviteId);

    /// Sets an invite's status and its time, as the invitee's reply writes them.
    void SetInviteReply(uint64 inviteId, uint32 status, uint32 lastUpdateTime);

    /// Sets an invite's status and its time, as the status handler writes them.
    void SetInviteStatus(uint64 inviteId, uint32 status, uint32 lastUpdateTime);

    /// Sets an invite's moderation rank.
    void SetInviteRank(uint64 inviteId, uint32 rank);
}

#endif
