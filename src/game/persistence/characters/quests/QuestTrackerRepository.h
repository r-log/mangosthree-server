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

#ifndef MANGOS_H_QUESTTRACKERREPOSITORY
#define MANGOS_H_QUESTTRACKERREPOSITORY

#include "Platform/Define.h"

/// The quest tracker's rows (`quest_tracker`), one per quest a character accepted, written when
/// the tracker is enabled. Every write is queued; the updates touch the character's latest
/// acceptance of the quest.
namespace QuestTrackerRepository
{
    /// Records a quest's acceptance now, with the core's hash and revision.
    void InsertAccept(uint32 questId, uint32 guid, char const* coreHash, char const* coreRevision);

    /// Each stamps the latest acceptance: completed now, abandoned now, completed by a GM.
    void SetCompleteTime(uint32 questId, uint32 guid);
    void SetAbandonTime(uint32 questId, uint32 guid);
    void SetCompletedByGm(uint32 questId, uint32 guid);
}

#endif
