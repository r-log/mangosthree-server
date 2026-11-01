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

#ifndef MANGOS_H_QUESTNOTICES
#define MANGOS_H_QUESTNOTICES

#include "ItemPrototype.h"
#include "ObjectGuid.h"
#include "Platform/Define.h"

#include <vector>

class Player;
class Quest;

/// A quest giver's dialog status a client is told: the giver and its status.
struct QuestGiverStatusEntry
{
    ObjectGuid guid;
    uint32 status = 0;
};

/**
 * What a player's quests tell its own client, one call per notice, and a shared quest's offer to the
 * player it is shared with. The player keeps when each is told. The session installs the notices that
 * send them at start-up; this base sends nothing.
 */
class QuestNotices
{
    public:
        virtual ~QuestNotices();

        /// `player` is told its quest log is full.
        virtual void QuestLogFull(Player const& player);
        /// `player` is told the event objective of its quest `questId` is complete.
        virtual void QuestCompleteEvent(Player const& player, uint32 questId);
        /// `player` is told its quest `questId` failed for `reason`.
        virtual void QuestFailed(Player const& player, uint32 questId, InventoryResult reason);
        /// `player` is told the timer of its quest `questId` ran out.
        virtual void QuestTimerFailed(Player const& player, uint32 questId);
        /// `player` is told why it cannot take a quest.
        virtual void QuestInvalid(Player const& player, uint32 reason);
        /// `receiver` is asked to accept the quest `quest` that `sharer` shares, its title in the receiver's
        /// locale.
        virtual void QuestConfirmAccept(Player const& receiver, Quest const& quest, ObjectGuid sharer);
        /// `player` is told the result `result` of sharing a quest with `target`.
        virtual void QuestPushResult(Player const& player, ObjectGuid target, uint8 result);
        /// `player` is told its quest `questId` counts `count` of `required` kills or uses of `entry`, the
        /// last one `guid`.
        virtual void QuestKillCredited(Player const& player, uint32 questId, uint32 entry, uint32 count, uint32 required, ObjectGuid guid);
        /// `player` is told the dialog status of each of `statuses`' quest givers.
        virtual void QuestGiverStatuses(Player const& player, std::vector<QuestGiverStatusEntry> const& statuses);

        /// The installed notices, or the base when none are installed.
        static QuestNotices& Installed();
        /// Installs `notices` and answers the ones it replaces (NULL for the base). NULL installs the
        /// base. Installed once at start-up, before the world loads.
        static QuestNotices* Install(QuestNotices* notices);
};

#define sQuestNotices QuestNotices::Installed()

#endif
