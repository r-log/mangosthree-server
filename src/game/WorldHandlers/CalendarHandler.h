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

#ifndef MANGOS_H_CALENDARHANDLER
#define MANGOS_H_CALENDARHANDLER

#include "Platform/Define.h"

#include <ctime>

class CalendarEvent;
class CalendarInvite;
class Player;
class WorldPacket;

/// The calendar packets only the session sends: an invite's status, its moderator flag and an event's
/// update, each to every client the event concerns, and the client's pending action cleared.
void SendCalendarEventStatus(CalendarInvite const* invite);
void SendCalendarClearPendingAction(Player* player);
void SendCalendarEventModeratorStatusAlert(CalendarInvite const* invite);
void SendCalendarEventUpdateAlert(CalendarEvent const* event, time_t oldEventTime);
/// Sends `packet` to the event's guild for a guild event, and to every online invitee outside it.
void SendPacketToAllEventRelatives(WorldPacket packet, CalendarEvent const* event);
/// Installs the calendar notices, which send the packets the calendar reports.
void InstallCalendarNotices();

#endif
