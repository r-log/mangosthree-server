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

#include "entities/ChatNotices.h"

#include <atomic>
#include <cstddef>

namespace
{
    std::atomic<ChatNotices*> s_chatNoticesInstalled(NULL);

    ChatNotices& BaseChatNotices()
    {
        static ChatNotices notices;
        return notices;
    }
}

ChatNotices::~ChatNotices()
{
}

void ChatNotices::MonsterSaid(WorldObject const& /*speaker*/, ChatMsg /*msgtype*/, char const* /*text*/, Unit const* /*target*/, float /*range*/)
{
}

void ChatNotices::MonsterTextAround(WorldObject const& /*speaker*/, MangosStringLocale const& /*text*/, ChatMsg /*msgtype*/, Language /*language*/, Unit const* /*target*/, float /*range*/)
{
}

void ChatNotices::MonsterTextTo(WorldObject const& /*speaker*/, MangosStringLocale const& /*text*/, ChatMsg /*msgtype*/, Language /*language*/, Unit const* /*target*/, std::vector<Player const*> const& /*listeners*/)
{
}

void ChatNotices::Said(Player& /*speaker*/, ChatMsg /*msgtype*/, std::string const& /*text*/, uint32 /*language*/, float /*range*/, bool /*ownTeamOnly*/)
{
}

void ChatNotices::Whispered(Player& /*speaker*/, Player& /*receiver*/, std::string const& /*text*/, uint32 /*language*/)
{
}

void ChatNotices::AutoReplied(Player& /*speaker*/, Player const& /*receiver*/, ChatMsg /*msgtype*/)
{
}

ChatNotices& ChatNotices::Installed()
{
    ChatNotices* notices = s_chatNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseChatNotices();
}

ChatNotices* ChatNotices::Install(ChatNotices* notices)
{
    return s_chatNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
