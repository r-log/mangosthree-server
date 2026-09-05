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
#include "wire/MovementElements.h"
#include "wire/MovementStatus.h"

#include <functional>
#include <map>
#include <vector>

namespace loadtest
{
    /// A server-initiated movement change and the ack the client owes for it.
    struct ChangePair
    {
        uint16 change;
        uint16 ack;
    };

    /// The 4.3.4 pairs, by name from Opcodes.h. Data, not policy.
    const std::vector<ChangePair>& KnownChangePairs();

    /**
     * @brief Answers server-initiated movement changes under a scripted policy.
     *
     * Decodes the change with the layout the lookup returns, and encodes the ack
     * with the ack's layout, echoing the decoded status (the real client echoes
     * its own status; for a session that does not move between the two, the
     * change's status is that status). A change or ack with no registered layout
     * is counted as unregistered; a change whose layout is registered but does
     * not decode is counted apart, as a decode failure -- the first list is what
     * P1 has to register, the second what it has to fix. An ack is stamped with
     * the peer's own clock, as a real client's would be.
     */
    class AckEngine
    {
        public:
            typedef std::function<Wire::Sequence(uint16)> Lookup;

            AckEngine(const AckPolicy& policy, Lookup lookup,
                      std::vector<ChangePair> pairs = KnownChangePairs());

            bool IsChange(uint16 opcode) const;

            /// Decode `change` and queue its ack per the policy. Returns false, and
            /// counts the opcode, when no layout is registered for it or its ack.
            bool Plan(const WorldPacket& change, uint32 nowTicks);

            /// Acks whose time has come, encoded, in the order they were planned.
            std::vector<WorldPacket> Due(uint32 nowTicks);

            uint32 Sent() const { return m_sent; }
            uint32 Dropped() const { return m_dropped; }
            const std::map<uint16, uint32>& Unregistered() const { return m_unregistered; }
            const std::map<uint16, uint32>& DecodeFailures() const { return m_decodeFailures; }

        private:
            struct Pending
            {
                uint16               opcode;
                Wire::MovementStatus status;
                uint32               dueTicks;
            };

            const ChangePair* Find(uint16 change) const;

            AckPolicy               m_policy;
            Lookup                  m_lookup;
            std::vector<ChangePair> m_pairs;
            std::vector<Pending>    m_pending;
            std::map<uint16, uint32> m_unregistered;
            std::map<uint16, uint32> m_decodeFailures;
            uint32                  m_sent = 0;
            uint32                  m_dropped = 0;
    };
}
