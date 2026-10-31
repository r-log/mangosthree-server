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

#include "MovementInfo.h"
#include "MovementBridge.h"
#include "WireParity.h"
#include "Geometry/Placement.h"
#include "Log.h"
#include "WorldPacket.h"
#include "wire/MovementCodec.h"
#include "wire/MovementSequences.h"

void MovementInfo::Read(ByteBuffer& data, uint16 opcode)
{
    Wire::Sequence const sequence = Wire::SequenceFor(opcode);
    if (!sequence)
    {
        // An opcode with no layout leaves the record as it was and says so. Every
        // opcode a handler reads through here has a registry row.
        sLog.outError("Unsupported MovementInfo::Read for 0x%X (%s)!", opcode, WireParity::OpcodeName(opcode));
        return;
    }
    Wire::MovementStatus status;
    Wire::DecodeResult const result = Wire::Decode(data, sequence, status);
    if (!result.ok())
    {
        // A packet the codec cannot decode is counted and thrown as a bad packet:
        // WorldSession::Update logs it and, by config, kicks.
        WireParity::Rejected(opcode, result.error);
        throw ByteBufferException(false, data.rpos(), 0, data.size());
    }
    Movement::FromWire(status, *this);
    WireParity::BridgeCheck(opcode, status, *this);
}

/**
 * @brief Serializes movement information into a packet buffer.
 *
 * @param data The packet buffer to populate.
 */
void MovementInfo::Write(ByteBuffer& data, uint16 opcode) const
{
    Wire::Sequence const sequence = Wire::SequenceFor(opcode);
    if (!sequence)
    {
        sLog.outError("Unsupported MovementInfo::Write for 0x%X (%s)!", opcode, WireParity::OpcodeName(opcode));
        return;
    }
    Wire::MovementStatus status = Movement::ToWire(*this);
    // Both orientation slots go out wrapped into [0, 2pi): the relays
    // (SMSG_PLAYER_MOVE, SMSG_MOVE_UPDATE_KNOCK_BACK) forward a client-sourced
    // record, and VerifyMovementInfo only demands finiteness.
    status.pos.o = Geometry::Placement::NormalizeOrientation(status.pos.o);
    status.transport.pos.o = Geometry::Placement::NormalizeOrientation(status.transport.pos.o);
    // These four go out clear, even when a relay forwards a client-sourced
    // record that set them: the unnamed bit is 0, the flags-block presence is
    // derived from the flags themselves, and there is no height-change failure.
    status.has.unknownBit = false;
    status.has.emptyFlagsBlock = false;
    status.has.emptyFlags2Block = false;
    status.has.heightChangeFailed = false;
    Wire::Encode(data, sequence, status);
}

WorldPacket& operator<< (WorldPacket& buf, MovementInfo const& mi)
{
    mi.Write(buf, buf.GetOpcode());
    return buf;
}

WorldPacket& operator>> (WorldPacket& buf, MovementInfo& mi)
{
    mi.Read(buf, buf.GetOpcode());
    return buf;
}
