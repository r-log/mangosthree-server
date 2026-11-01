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

#ifndef MANGOS_H_QUESTPACKETS
#define MANGOS_H_QUESTPACKETS

#include "entities/player/quests/QuestNotices.h"

/// A player's quest notices, each built here: to the player's own client, a shared quest's offer to
/// the player it is shared with.
class SessionQuestNotices : public QuestNotices
{
    public:
        void QuestLogFull(Player const& player) override;
        void QuestCompleteEvent(Player const& player, uint32 questId) override;
        void QuestFailed(Player const& player, uint32 questId, InventoryResult reason) override;
        void QuestTimerFailed(Player const& player, uint32 questId) override;
        void QuestInvalid(Player const& player, uint32 reason) override;
        void QuestConfirmAccept(Player const& receiver, Quest const& quest, ObjectGuid sharer) override;
        void QuestPushResult(Player const& player, ObjectGuid target, uint8 result) override;
        void QuestKillCredited(Player const& player, uint32 questId, uint32 entry, uint32 count, uint32 required, ObjectGuid guid) override;
        void QuestGiverStatuses(Player const& player, std::vector<QuestGiverStatusEntry> const& statuses) override;
};

#endif
