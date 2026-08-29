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
     *   send_slot  which stream the client transmits that opcode on. For an SMSG
     *              the client never takes that path, so using it to choose the
     *              server's egress link is an inference, not client law -- it is
     *              right for all 19 opcodes where the client can prove us wrong,
     *              and the fallback for an opcode outside the table is stream 0,
     *              never a guess onto stream 1.
     *
     *   recv_slot  whether the client's receive gate drops the opcode unless it
     *              arrives on stream 1. This one IS law: 19 opcodes carry it, and
     *              emitting any of them anywhere else means the client silently
     *              discards the packet. Fail closed instead.
     *
     * Everything here is a pure function of the opcode; there is no state and no
     * allocation, so it is safe from any thread.
     */

    /// The stream the server emits this opcode on. Unknown opcode -> Zero.
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

