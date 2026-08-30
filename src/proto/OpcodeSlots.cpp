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

#include "OpcodeSlots.h"

#include <array>
#include <cstddef>

namespace proto
{
    namespace
    {
        enum SlotFlags : uint8
        {
            SLOT_KNOWN     = 0x01,
            SLOT_SEND_ONE  = 0x02,
            SLOT_RECV_GATE = 0x04
        };

        /**
         * @brief One flag byte per opcode, indexed directly by the opcode.
         *
         * 64 KB of read-only data buys a branchless lookup on a path every
         * outgoing packet crosses. The alternative -- a sorted table and a binary
         * search over 1252 entries -- costs ten dependent loads per send to save
         * memory the process will never miss.
         */
        using SlotTable = std::array<uint8, 0x10000>;

        SlotTable BuildSlotTable()
        {
            SlotTable table{};

#define SLOT(op, send, recv) \
            table[(op)] = uint8(SLOT_KNOWN | \
                                ((send) ? SLOT_SEND_ONE : 0) | \
                                ((recv) ? SLOT_RECV_GATE : 0));
#include "OpcodeSlots.inc"
#undef SLOT

            return table;
        }

        const SlotTable& Slots()
        {
            static const SlotTable table = BuildSlotTable();
            return table;
        }

        /// The connection-control class, as the client selects it: it tests
        /// (opcode & 0xB3FD) == 0x0140 ahead of every other dispatch class,
        /// which picks out exactly eight catalog opcodes.
        const uint16 CONTROL_MASK  = 0xB3FD;
        const uint16 CONTROL_VALUE = 0x0140;
    }

    LinkSlot ServerSlotOf(uint16 opcode)
    {
        return (Slots()[opcode] & SLOT_RECV_GATE) != 0 ? LinkSlot::One : LinkSlot::Zero;
    }

    LinkSlot SendSlotOf(uint16 opcode)
    {
        return (Slots()[opcode] & SLOT_SEND_ONE) != 0 ? LinkSlot::One : LinkSlot::Zero;
    }

    bool IsRecvGated(uint16 opcode)
    {
        return (Slots()[opcode] & SLOT_RECV_GATE) != 0;
    }

    bool IsTransportPlane(uint16 opcode)
    {
        return (opcode & CONTROL_MASK) == CONTROL_VALUE;
    }

    bool IsKnownSlotOpcode(uint16 opcode)
    {
        return (Slots()[opcode] & SLOT_KNOWN) != 0;
    }
}
