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

#pragma once

#include "Platform/Define.h"

namespace proto
{
    /**
     * @brief One of the two logical world streams a 15595 client holds open.
     *
     * The client's connection block has four connection entries but only two
     * addressable stream indices: entries 2 and 3 are staging slots used while a
     * redirect is in flight, and the client hard-refuses an SMSG_CONNECT_TO that
     * names any target but 0 or 1. There is deliberately no Two/Three here --
     * naming them would invite the belief that a third stream can be opened.
     */
    enum class LinkSlot : uint8
    {
        Zero = 0,
        One  = 1
    };

    /**
     * @brief What one physical TCP connection is currently being used for.
     *
     * Distinct from LinkSlot on purpose. A connection accepted on the stream-1
     * port is a real socket long before it is stream 1: it runs the banner
     * exchange while the client still holds the stream, and becomes LinkSlot::One
     * only once the client has promoted it and started framing packets on it.
     * Writing to it as the logical stream in between is how a packet meant for
     * stream 1 ends up in a client-side queue that is never flushed.
     */
    enum class ConnRole : uint8
    {
        Live0,     ///< the stream-0 connection, the one that authenticates
        Staging1,  ///< accepted on the stream-1 port, not yet promoted
        Live1      ///< promoted; the session's LinkSlot::One
    };
}

