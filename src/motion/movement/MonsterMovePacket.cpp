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

#include "MonsterMovePacket.h"
#include "MonsterMoveStop.h"
#include "MoveSpline.h"
#include "packet_builder.h"
#include "Opcodes.h"
#include "WorldPacket.h"

namespace Movement
{
    static_assert(kMonsterMoveStopType == MonsterMoveStop, "the stop writer's type byte drifted from MoveSpline.h");

    namespace
    {
        uint16 OpcodeFor(Aboard const& aboard)
        {
            return aboard.OnBoard() ? SMSG_MONSTER_MOVE_TRANSPORT : SMSG_MONSTER_MOVE;
        }
    }

    void BuildMonsterMove(WorldPacket& out, ObjectGuid mover, Aboard const& aboard, MoveSpline const& spline)
    {
        out.Initialize(OpcodeFor(aboard), 64);
        out << mover.WriteAsPacked();
        if (aboard.OnBoard())
        {
            out << aboard.vessel.WriteAsPacked();
            out << int8(aboard.seat);
        }
        PacketBuilder::WriteMonsterMove(spline, out);
    }

    void BuildMonsterMoveStop(WorldPacket& out, ObjectGuid mover, Aboard const& aboard, Location const& at, uint32 splineId)
    {
        out.Initialize(OpcodeFor(aboard), 64);
        WriteMonsterMoveStop(out, mover.GetRawValue(), aboard.OnBoard(), aboard.vessel.GetRawValue(), aboard.seat,
                             at.x, at.y, at.z, splineId);
    }
}
