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

#include "Peer.hpp"
#include "Platform/Define.h"
#include "WorldPacket.h"

#include <vector>

namespace loadtest
{
    /**
     * @brief Moves a character in a straight line the way the real client
     *        reports it: CMSG_MOVE_START_FORWARD once, MSG_MOVE_HEARTBEAT every
     *        half second, CMSG_MOVE_STOP when the time is up.
     *
     * Pure: fed the clock, it returns the packets due; the caller sends them.
     * The first Advance() call fixes the moment the lead starts counting.
     */
    class Walker
    {
        public:
            Walker(const WalkScript& script, uint64 guid, const Wire::Vec4& start);

            /// Packets due by `nowTicks`, in order; empty when nothing is due.
            std::vector<WorldPacket> Advance(uint32 nowTicks);

            bool Started() const { return m_started; }
            bool Done() const { return m_done; }
            const Wire::Vec4& Position() const { return m_pos; }
            uint32 Starts() const { return m_starts; }
            uint32 Heartbeats() const { return m_heartbeats; }
            uint32 Stops() const { return m_stops; }

        private:
            WorldPacket Packet(uint16 opcode, uint32 flags, uint32 nowTicks) const;

            WalkScript m_script;
            uint64     m_guid;
            Wire::Vec4 m_pos;
            float      m_heading;

            bool   m_armed = false;      ///< first Advance seen; the lead is counting
            bool   m_started = false;
            bool   m_done = false;
            uint32 m_armedTicks = 0;
            uint32 m_startTicks = 0;
            uint32 m_lastTicks = 0;
            uint32 m_nextHeartbeat = 0;
            uint32 m_starts = 0;
            uint32 m_heartbeats = 0;
            uint32 m_stops = 0;
    };
}
