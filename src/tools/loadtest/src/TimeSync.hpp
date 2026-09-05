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
#include "WorldPacket.h"

#include <chrono>

namespace loadtest
{
    /**
     * @brief The client's millisecond tick clock.
     *
     * A real client stamps movement `time` and answers CMSG_TIME_SYNC_RESP from
     * the same counter, and the server derives its clock delta from that pair.
     * One clock object per session keeps the two consistent.
     */
    class ClientClock
    {
        public:
            explicit ClientClock(uint32 base = 100000);
            /// Milliseconds since construction plus the base (a real tick count is
            /// never near zero either).
            uint32 Ticks() const;

        private:
            uint32 m_base;
            std::chrono::steady_clock::time_point m_start;
    };

    /// SMSG_TIME_SYNC_REQ is one uint32: the server's counter.
    bool ReadTimeSyncRequest(const WorldPacket& packet, uint32& counter);

    /// CMSG_TIME_SYNC_RESP echoes the counter and reports the client's ticks.
    WorldPacket MakeTimeSyncResponse(uint32 counter, uint32 clientTicks);
}
