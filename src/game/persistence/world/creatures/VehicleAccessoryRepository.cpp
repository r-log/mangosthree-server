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

#include "VehicleAccessoryRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace VehicleAccessoryRepository
{
    VehicleAccessoryRow ReadRow(Field* fields)
    {
        VehicleAccessoryRow row;
        row.vehicleEntry = fields[0].GetUInt32();
        row.seat = fields[1].GetUInt32();
        row.accessoryEntry = fields[2].GetUInt32();
        return row;
    }

    TableRead<VehicleAccessoryRow> QueryVehicleAccessories()
    {
        //                                               0                1       2
        return ReadWorldTable<VehicleAccessoryRow>("SELECT `vehicle_entry`, `seat`, `accessory_entry` FROM `vehicle_accessory`", ReadRow);
    }
}
