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

#include "entities/player/quests/QuestNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<QuestNotices*> s_questNoticesInstalled(NULL);

    QuestNotices& BaseQuestNotices()
    {
        static QuestNotices notices;
        return notices;
    }
}

QuestNotices::~QuestNotices()
{
}

void QuestNotices::QuestLogFull(Player const& /*player*/)
{
}

void QuestNotices::QuestCompleteEvent(Player const& /*player*/, uint32 /*questId*/)
{
}

void QuestNotices::QuestFailed(Player const& /*player*/, uint32 /*questId*/, InventoryResult /*reason*/)
{
}

void QuestNotices::QuestTimerFailed(Player const& /*player*/, uint32 /*questId*/)
{
}

void QuestNotices::QuestInvalid(Player const& /*player*/, uint32 /*reason*/)
{
}

void QuestNotices::QuestConfirmAccept(Player const& /*receiver*/, Quest const& /*quest*/, ObjectGuid /*sharer*/)
{
}

void QuestNotices::QuestPushResult(Player const& /*player*/, ObjectGuid /*target*/, uint8 /*result*/)
{
}

void QuestNotices::QuestKillCredited(Player const& /*player*/, uint32 /*questId*/, uint32 /*entry*/, uint32 /*count*/, uint32 /*required*/, ObjectGuid /*guid*/)
{
}

void QuestNotices::QuestGiverStatuses(Player const& /*player*/, std::vector<QuestGiverStatusEntry> const& /*statuses*/)
{
}

QuestNotices& QuestNotices::Installed()
{
    QuestNotices* notices = s_questNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseQuestNotices();
}

QuestNotices* QuestNotices::Install(QuestNotices* notices)
{
    return s_questNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
