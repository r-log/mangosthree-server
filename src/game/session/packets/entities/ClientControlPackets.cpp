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

#include "ClientControlPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "wire/MoverCodec.h"

void BuildClientControlUpdatePacket(WorldPacket& packet, ControlUpdateFact const& fact)
{
    packet.Initialize(SMSG_CLIENT_CONTROL_UPDATE, 10);
    Wire::ControlUpdate cu;
    cu.guid = fact.unit.GetRawValue();
    cu.allowMove = fact.allowMove;
    Wire::EncodeControlUpdate(packet, cu);
}

void BuildMoveSetActiveMoverPacket(WorldPacket& packet, ActiveMoverFact const& fact)
{
    packet.Initialize(SMSG_MOVE_SET_ACTIVE_MOVER, 9);
    Wire::ActiveMover mover;
    mover.guid = fact.mover.GetRawValue();
    Wire::EncodeActiveMover(packet, SMSG_MOVE_SET_ACTIVE_MOVER, mover);
}

void BuildTimeSyncRequestPacket(WorldPacket& packet, TimeSyncRequestFact const& fact)
{
    packet.Initialize(SMSG_TIME_SYNC_REQ, 4);
    packet << uint32(fact.counter);
}
