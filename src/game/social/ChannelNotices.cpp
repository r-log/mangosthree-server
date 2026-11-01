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

#include "social/ChannelNotices.h"

#include <atomic>

namespace
{
    std::atomic<ChannelNotices*> s_channelNoticesInstalled(NULL);

    ChannelNotices& BaseChannelNotices()
    {
        static ChannelNotices notices;
        return notices;
    }
}

ChannelNotices::~ChannelNotices()
{
}

void ChannelNotices::ToOne(Channel& /*channel*/, ObjectGuid /*who*/, ChannelNotice const& /*notice*/)
{
}

void ChannelNotices::ToAll(Channel& /*channel*/, ChannelNotice const& /*notice*/, ObjectGuid /*ignoring*/)
{
}

void ChannelNotices::NotMember(Player& /*player*/, std::string const& /*channelName*/)
{
}

void ChannelNotices::MemberList(Channel& /*channel*/, ObjectGuid /*to*/, std::vector<std::pair<ObjectGuid, uint8> > const& /*members*/)
{
}

void ChannelNotices::UserListChanged(Channel& /*channel*/, ObjectGuid /*who*/)
{
}

void ChannelNotices::UserListRemoved(Channel& /*channel*/, ObjectGuid /*who*/)
{
}

void ChannelNotices::Said(Channel& /*channel*/, Player& /*speaker*/, char const* /*text*/, uint32 /*language*/, ObjectGuid /*ignoring*/)
{
}

ChannelNotices& ChannelNotices::Installed()
{
    ChannelNotices* notices = s_channelNoticesInstalled.load(std::memory_order_acquire);
    return notices ? *notices : BaseChannelNotices();
}

ChannelNotices* ChannelNotices::Install(ChannelNotices* notices)
{
    return s_channelNoticesInstalled.exchange(notices, std::memory_order_acq_rel);
}
