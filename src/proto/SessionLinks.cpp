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

#include "SessionLinks.h"

#include "Log/Log.h"
#include "OpcodeSlots.h"

#include <utility>

namespace proto
{
    namespace
    {
        /**
         * @brief How many stream-1 packets to hold before dropping them.
         *
         * The queue exists for the promotion window, which is one round trip
         * long. A session that has accumulated this many packets in it is not
         * waiting for promotion any more -- it is a session whose second stream
         * is never coming up, and holding an unbounded queue for it trades a
         * visible fault for an invisible leak.
         */
        const size_t MAX_HELD_PACKETS = 4096;

        const std::string EMPTY_ADDRESS;
    }

    SessionLinks::SessionLinks(std::shared_ptr<IClientLink> streamZero)
        : m_zero(std::move(streamZero))
    {
    }

    void SessionLinks::SendPacket(const WorldPacket& packet)
    {
        // The client's receive gate decides this, and nothing else: nineteen
        // opcodes must arrive on stream 1, everything else belongs on stream 0.
        // Routing by the client's own send router instead put the whole login
        // sequence on the second stream, where the client dropped the stream and
        // answered with disconnect reason 3 (2026-08-30). See OpcodeSlots.h.
        SendOn(ServerSlotOf(packet.GetOpcode()), packet);
    }

    void SessionLinks::SendOn(LinkSlot slot, const WorldPacket& packet)
    {
        const uint16 opcode = packet.GetOpcode();

        // Fail closed rather than emit one of the nineteen where the client's
        // receive gate will drop it. A dropped ATTACK_START is not a dropped
        // packet to the player -- it is a mob that never enters combat, debugged
        // for a week as a combat bug.
        if (IsRecvGated(opcode) && slot != LinkSlot::One)
        {
            {
                std::lock_guard<std::mutex> lock(m_lock);
                ++m_counters.slotGateViolations;
            }

            sLog.outError("proto: refused to send stream-1-only opcode 0x%.4X on stream 0",
                          opcode);
            return;
        }

        std::shared_ptr<IClientLink> link;
        {
            std::lock_guard<std::mutex> lock(m_lock);

            if (slot == LinkSlot::Zero)
            {
                link = m_zero;
            }
            else if (m_one)
            {
                link = m_one;
            }
            else
            {
                QueueForSlotOne(packet);
                return;
            }
        }

        if (link)
        {
            link->SendPacket(packet);
        }
    }

    void SessionLinks::QueueForSlotOne(const WorldPacket& packet)
    {
        ++m_counters.emittedBeforeLive;

        if (m_held.size() >= MAX_HELD_PACKETS)
        {
            ++m_counters.droppedWhileHeld;
            return;
        }

        m_held.push_back(packet);
    }

    bool SessionLinks::AttachSlotOne(const std::shared_ptr<IClientLink>& link, uint32 generation,
                                     const std::function<void()>& announce)
    {
        std::deque<WorldPacket> release;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            if (generation != m_expectedGeneration)
            {
                return false;
            }

            // The generation is good, so this socket is going to be the stream;
            // tell the client so before it can receive anything else. Under the
            // lock, because publishing m_one below is what lets other threads
            // send here, and the client will refuse ordinary traffic until this
            // has arrived.
            if (announce)
            {
                announce();
            }

            m_one = link;
            release.swap(m_held);
        }
        for (const WorldPacket& packet : release)
        {
            link->SendPacket(packet);
        }
        return true;
    }

    void SessionLinks::ExpectSlotOne(uint32 generation)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_expectedGeneration = generation;
    }

    void SessionLinks::DetachSlotOne(const IClientLink* link)
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_one.get() == link)
        {
            m_one.reset();
        }
    }

    void SessionLinks::Close()
    {
        std::shared_ptr<IClientLink> zero;
        std::shared_ptr<IClientLink> one;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            zero = m_zero;
            one  = m_one;
            m_held.clear();
        }

        if (one)
        {
            one->Close();
        }
        if (zero)
        {
            zero->Close();
        }
    }

    const std::string& SessionLinks::GetRemoteAddress() const
    {
        std::lock_guard<std::mutex> lock(m_lock);
        return m_zero ? m_zero->GetRemoteAddress() : EMPTY_ADDRESS;
    }

    bool SessionLinks::IsClosed() const
    {
        std::lock_guard<std::mutex> lock(m_lock);
        return !m_zero || m_zero->IsClosed();
    }

    bool SessionLinks::IsSlotLive(LinkSlot slot) const
    {
        std::lock_guard<std::mutex> lock(m_lock);

        if (slot == LinkSlot::Zero)
        {
            return m_zero && !m_zero->IsClosed();
        }

        return m_one && !m_one->IsClosed();
    }

    SessionLinks::Counters SessionLinks::GetCounters() const
    {
        std::lock_guard<std::mutex> lock(m_lock);
        return m_counters;
    }
}
