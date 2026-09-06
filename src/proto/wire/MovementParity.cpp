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

#include "wire/MovementParity.h"

namespace Wire
{
    // This list must track MovementStatus::operator== field for field: if that
    // operator gains a field, add it here too, in the struct's declaration order.
    char const* FirstDifference(MovementStatus const& a, MovementStatus const& b)
    {
#define WIRE_DIFF(field) if (!(a.field == b.field)) { return #field; }
        WIRE_DIFF(guid)
        WIRE_DIFF(guid2)
        WIRE_DIFF(flags)
        WIRE_DIFF(flags2)
        WIRE_DIFF(time)
        WIRE_DIFF(pos.x)
        WIRE_DIFF(pos.y)
        WIRE_DIFF(pos.z)
        WIRE_DIFF(pos.o)
        WIRE_DIFF(pitch)
        WIRE_DIFF(splineElevation)
        WIRE_DIFF(counter)
        WIRE_DIFF(byteParam)
        WIRE_DIFF(value)
        WIRE_DIFF(twoBits)
        WIRE_DIFF(has.orientation)
        WIRE_DIFF(has.pitch)
        WIRE_DIFF(has.timestamp)
        WIRE_DIFF(has.spline)
        WIRE_DIFF(has.splineElevation)
        WIRE_DIFF(has.unknownBit)
        WIRE_DIFF(has.emptyFlagsBlock)
        WIRE_DIFF(has.emptyFlags2Block)
        WIRE_DIFF(has.heightChangeFailed)
        WIRE_DIFF(fall.present)
        WIRE_DIFF(fall.hasDirection)
        WIRE_DIFF(fall.time)
        WIRE_DIFF(fall.vertical)
        WIRE_DIFF(fall.horizontal)
        WIRE_DIFF(fall.cosAngle)
        WIRE_DIFF(fall.sinAngle)
        WIRE_DIFF(transport.present)
        WIRE_DIFF(transport.guid)
        WIRE_DIFF(transport.pos.x)
        WIRE_DIFF(transport.pos.y)
        WIRE_DIFF(transport.pos.z)
        WIRE_DIFF(transport.pos.o)
        WIRE_DIFF(transport.time)
        WIRE_DIFF(transport.time2)
        WIRE_DIFF(transport.vehicleId)
        WIRE_DIFF(transport.seat)
        WIRE_DIFF(transport.hasTime2)
        WIRE_DIFF(transport.hasVehicleId)
#undef WIRE_DIFF
        return nullptr;
    }
}
