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

#include "LinkSlot.h"
#include "Platform/Define.h"

namespace proto
{
    /**
     * @brief Which of the two world streams an opcode belongs to.
     *
     * The 15595 client routes by opcode, not by socket. Two independent bits per
     * opcode were read out of the binary and generated into OpcodeSlots.inc:
     *
     *   send_slot  which stream the client transmits that opcode on. It says
     *              nothing about where the server should send: for an SMSG the
     *              client never takes that path, and the table still carries a
     *              bit for it because the router it was read from is indexed by
     *              every opcode. 729 opcodes have it set.
     *
     *   recv_slot  whether the client's receive gate drops the opcode unless it
     *              arrives on stream 1. This one IS law: 19 opcodes carry it, and
     *              emitting any of them anywhere else means the client silently
     *              discards the packet. Fail closed instead.
     *
     * Routing the server's egress by send_slot was an inference, and the wire
     * disproved it on 2026-08-30: the login sequence -- SMSG_ADDON_INFO,
     * SMSG_CACHE_VERSION, SMSG_TUTORIAL_FLAGS, SMSG_ACCOUNT_DATA_TIMES, all
     * send_slot 1 and recv_slot 0 -- went out on the second stream, and the
     * client answered by dropping that stream and reporting disconnect reason 3.
     * The client's own receive path (sub_1400AA9A0) consults ONLY the gate: an
     * opcode outside those 19 is taken on whichever connection it arrives on,
     * and the login sequence has always belonged on stream 0. So the server
     * routes by ServerSlotOf, and send_slot describes the client alone.
     *
     * Everything here is a pure function of the opcode; there is no state and no
     * allocation, so it is safe from any thread.
     */

    /// The stream the SERVER must emit this opcode on: One for the 19 the
    /// client's gate insists on, Zero for everything else, known or not.
    LinkSlot ServerSlotOf(uint16 opcode);

    /// The stream the CLIENT transmits this opcode on. Describes the client's
    /// own router; it is not the server's egress rule -- see ServerSlotOf.
    LinkSlot SendSlotOf(uint16 opcode);

    /// True for the 19 opcodes the client accepts only on stream 1.
    bool IsRecvGated(uint16 opcode);

    /**
     * @brief True for the eight connection-control opcodes.
     *
     * These are dispatched by the client before the receive gate is consulted and
     * are accepted on any connection, live or staging. They are the only opcodes
     * that may be written to a connection that is not yet a logical stream, and
     * they are addressed by connection rather than by slot.
     */
    bool IsTransportPlane(uint16 opcode);

    /// False for an opcode the generated table does not name.
    bool IsKnownSlotOpcode(uint16 opcode);
}

