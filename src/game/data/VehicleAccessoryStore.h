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

#ifndef MANGOS_H_VEHICLEACCESSORYSTORE
#define MANGOS_H_VEHICLEACCESSORYSTORE

#include "Platform/Define.h"

#include <map>
#include <utility>

struct VehicleAccessory
{
    uint32 vehicleEntry;
    uint32 seatId;
    uint32 passengerEntry;
};

/**
 * The vehicle accessories (`vehicle_accessory`): the passengers boarded onto a vehicle, per vehicle
 * entry, in the order they were added. Erasing a vehicle forgets every one of its passengers, and
 * clearing forgets every vehicle.
 */
class VehicleAccessoryStore
{
    public:
        typedef std::multimap<uint32, VehicleAccessory> VehicleAccessoryMap;
        typedef std::pair<VehicleAccessoryMap::const_iterator, VehicleAccessoryMap::const_iterator> Bounds;

        void Clear();
        void Add(VehicleAccessory const& accessory);
        /// The passengers of `vehicleEntry`, in the order they were added.
        Bounds GetBounds(uint32 vehicleEntry) const;
        void Erase(uint32 vehicleEntry);
        uint32 GetCount() const;

    private:
        VehicleAccessoryMap m_accessories;
};

#endif
