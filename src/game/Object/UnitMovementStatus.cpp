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

#include "Unit.h"
#include "KinematicFlags.h"
#include "State.h"
#include "Vehicle.h"

namespace Movement
{
    uint32 FlagsFor(Motion::Kinematics const& state)
    {
        uint32 flags = MOVEFLAG_NONE;
        if (state.root)            { flags |= MOVEFLAG_ROOT; }
        if (state.canFly)          { flags |= MOVEFLAG_CAN_FLY; }
        if (state.waterWalk)       { flags |= MOVEFLAG_WATERWALKING; }
        if (state.featherFall)     { flags |= MOVEFLAG_SAFE_FALL; }
        if (state.hover)           { flags |= MOVEFLAG_HOVER; }
        if (state.gravityDisabled) { flags |= MOVEFLAG_LEVITATING; }
        if (state.walk)            { flags |= MOVEFLAG_WALK_MODE; }
        if (state.swim)            { flags |= MOVEFLAG_SWIMMING; }
        return flags;
    }
}

uint32 Unit::WireFlags() const
{
    const uint32 flags = MotionState().GetMode() == Motion::Mode::ClientDriven
        ? uint32(m_movementInfo.GetMovementFlags())
        : Movement::FlagsFor(MotionState().Confirmed());
    return IsSplineEnabled() ? flags | MOVEFLAG_FORWARD : flags;
}

MovementInfo Unit::WireStatus() const
{
    if (MotionState().GetMode() == Motion::Mode::ClientDriven)
    {
        MovementInfo status = m_movementInfo;
        status.SetMovementFlags(MovementFlags(WireFlags()));
        if (m_vehicleInfo)
        {
            status.AddMovementFlags2(MovementFlags2(VehicleMoveFlags2(*m_vehicleInfo->GetVehicleEntry())));
        }
        return status;
    }

    // A server-driven unit's word is the kernel's and its spline's; no client's pitch, fall or
    // report goes with it. Its position, time and seat are the record's, a vehicle's limits its
    // entry's.
    MovementInfo status;
    status.SetMovementFlags(MovementFlags(WireFlags()));
    status.AddMovementFlags2(m_movementInfo.GetMovementFlags2());
    if (m_vehicleInfo)
    {
        status.AddMovementFlags2(MovementFlags2(VehicleMoveFlags2(*m_vehicleInfo->GetVehicleEntry())));
    }
    Position const* pos = m_movementInfo.GetPos();
    status.ChangePosition(pos->x, pos->y, pos->z, pos->o);
    status.UpdateTime(m_movementInfo.GetTime());
    if (!m_movementInfo.GetTransportGuid().IsEmpty())
    {
        Position const* seat = m_movementInfo.GetTransportPos();
        status.SetTransportData(m_movementInfo.GetTransportGuid(), seat->x, seat->y, seat->z, seat->o,
                                m_movementInfo.GetTransportTime(), m_movementInfo.GetTransportSeat());
    }
    return status;
}

MovementInfo const& Unit::ClientStatus() const
{
    return m_movementInfo;
}

void Unit::AcceptClientStatus(MovementInfo const& status)
{
    m_movementInfo = status;
}

void Unit::ResetClientWord(MovementFlags flags)
{
    m_movementInfo.SetMovementFlags(flags);
}

void Unit::ClearClientFall()
{
    m_movementInfo.ClearFallData();
}

void Unit::SetTransportStatus(ObjectGuid transport, float x, float y, float z, float o, uint32 time, int8 seat)
{
    m_movementInfo.SetTransportData(transport, x, y, z, o, time, seat);
}

void Unit::ClearTransportStatus()
{
    m_movementInfo.ClearTransportData();
}
