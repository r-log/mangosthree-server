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

/// A vehicle's movement limits: one flags2 bit for each limit its entry sets, and none for the
/// entry flags that are not movement limits.
#include "TestHarness.h"
#include "Unit.h"
#include "Vehicle.h"
#include "DBCStructure.h"

#include <cstring>

namespace
{
    VehicleEntry Entry(uint32 flags)
    {
        VehicleEntry entry;
        std::memset(&entry, 0, sizeof(entry));
        entry.Flags = flags;
        return entry;
    }
}

TEST(VehicleMoveFlags2_one_bit_per_movement_limit)
{
    CHECK_EQ(VehicleMoveFlags2(Entry(0)), uint32(MOVEFLAG2_NONE));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_NO_STRAFE)), uint32(MOVEFLAG2_NO_STRAFE));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_NO_JUMPING)), uint32(MOVEFLAG2_NO_JUMPING));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_FULLSPEEDTURNING)), uint32(MOVEFLAG2_FULLSPEEDTURNING));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_ALLOW_PITCHING)), uint32(MOVEFLAG2_ALLOW_PITCHING));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_FULLSPEEDPITCHING)), uint32(MOVEFLAG2_FULLSPEEDPITCHING));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_FIXED_POSITION)), uint32(MOVEFLAG2_NONE));
    CHECK_EQ(VehicleMoveFlags2(Entry(VEHICLE_FLAG_NO_STRAFE | VEHICLE_FLAG_ALLOW_PITCHING)),
             uint32(MOVEFLAG2_NO_STRAFE | MOVEFLAG2_ALLOW_PITCHING));
}
