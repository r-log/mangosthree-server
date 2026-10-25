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

#ifndef MANGOS_H_GAMEEVENTSTATUSREPOSITORY
#define MANGOS_H_GAMEEVENTSTATUSREPOSITORY

#include "Platform/Define.h"

class QueryResult;

/// The game events left running at shutdown (`game_event_status`): the start-up read runs
/// synchronously; every write is queued on the character database.
namespace GameEventStatusRepository
{
    /// Selects every game event left running at shutdown.
    QueryResult* QueryActiveEvents();

    /// Deletes every saved running event.
    void Truncate();

    /// Deletes a stopped event's row.
    void DeleteEvent(uint16 eventId);

    /// Inserts a started event's row, unless it is there.
    void InsertEvent(uint16 eventId);
}

#endif
