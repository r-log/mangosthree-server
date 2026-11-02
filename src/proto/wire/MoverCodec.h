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

#ifndef MANGOS_WIRE_MOVERCODEC_H
#define MANGOS_WIRE_MOVERCODEC_H

#include "Platform/Define.h"
#include "wire/MovementCodec.h"

class ByteBuffer;

namespace Wire
{
    /// SMSG_MOVE_SET_ACTIVE_MOVER and its CMSG_SET_ACTIVE_MOVER ack: the same
    /// guid, in two different masked-guid orders per opcode.
    ///
    /// Provenance: CMSG_SET_ACTIVE_MOVER -- the tree's own handler
    /// (HandleSetActiveMover), CPP's SetActiveMover::Read, and the real-client
    /// families golden. SMSG_MOVE_SET_ACTIVE_MOVER -- CPP's MoveSetActiveMover::Write
    /// and the client's reader sub_140374180 through lift_client_reader.py, which
    /// agreed with it exactly; no writer in this tree sends one yet, so it has no
    /// golden line.
    struct ActiveMover
    {
        uint64 guid = 0;
    };

    void EncodeActiveMover(ByteBuffer& out, uint16 opcode, ActiveMover const& v);   // opcode picks the order
    DecodeResult DecodeActiveMover(ByteBuffer& in, uint16 opcode, ActiveMover& out);

    /// SMSG_CLIENT_CONTROL_UPDATE: a packed guid and a byte.
    ///
    /// Provenance: the tree's own writer (Player::SetClientControl) and the client's
    /// handler sub_1402431B0, which read the same two fields in the same order.
    /// None in the golden: this tree sends none at login.
    struct ControlUpdate
    {
        uint64 guid = 0;
        uint8  allowMove = 0;
    };

    void EncodeControlUpdate(ByteBuffer& out, ControlUpdate const& v);
    DecodeResult DecodeControlUpdate(ByteBuffer& in, ControlUpdate& out);

    /// CMSG_MOVE_TIME_SKIPPED and its MSG_MOVE_TIME_SKIPPED relay: the mover's guid and the
    /// milliseconds its client did not move it, in a different order per opcode.
    ///
    /// Provenance: the client's PlayerMoveTimeSkipped::Put (the time, mask 5,1,3,7,6,0,4,2, bytes
    /// 7,1,2,4,3,6,0,5) and its ClientMoveSkipTime::Read (mask 5,7,2,1,0,6,3,4, the time, bytes
    /// 0,4,6,2,1,5,7,3), whose handler adds the time to the unit's movement clock.
    struct TimeSkipped
    {
        uint64 guid = 0;
        uint32 skipped = 0;
    };

    void EncodeTimeSkipped(ByteBuffer& out, uint16 opcode, TimeSkipped const& v);   // opcode picks the order
    DecodeResult DecodeTimeSkipped(ByteBuffer& in, uint16 opcode, TimeSkipped& out);
}

#endif
