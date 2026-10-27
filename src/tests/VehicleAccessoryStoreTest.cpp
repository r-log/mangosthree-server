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

/// The vehicle accessories, added and looked up with no database: a vehicle with no passenger, a
/// vehicle's passengers in the order added (every field), vehicles side by side, an erased vehicle
/// losing every passenger, and a cleared store.

#include "TestHarness.h"
#include "data/VehicleAccessoryStore.h"

#include <vector>

static VehicleAccessory MakeAccessory(uint32 vehicle, uint32 seat, uint32 passenger)
{
    VehicleAccessory accessory;
    accessory.vehicleEntry = vehicle;
    accessory.seatId = seat;
    accessory.passengerEntry = passenger;
    return accessory;
}

static std::vector<VehicleAccessory> Passengers(VehicleAccessoryStore const& store, uint32 vehicle)
{
    std::vector<VehicleAccessory> passengers;
    VehicleAccessoryStore::Bounds bounds = store.GetBounds(vehicle);
    for (VehicleAccessoryStore::VehicleAccessoryMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        passengers.push_back(itr->second);
    }
    return passengers;
}

TEST(VehicleAccessoryStore_VehicleWithNoPassengerAnswersNothing)
{
    VehicleAccessoryStore store;
    CHECK(Passengers(store, 28781).empty());

    store.Add(MakeAccessory(28781, 0, 28786));
    CHECK(Passengers(store, 28780).empty());
    CHECK(Passengers(store, 28782).empty());
}

TEST(VehicleAccessoryStore_PassengersComeBackInTheOrderAdded)
{
    VehicleAccessoryStore store;
    store.Add(MakeAccessory(500, 2, 9002));
    store.Add(MakeAccessory(500, 0, 9000));
    store.Add(MakeAccessory(500, 1, 9001));

    std::vector<VehicleAccessory> passengers = Passengers(store, 500);
    REQUIRE(passengers.size() == 3);
    CHECK_EQ(passengers[0].vehicleEntry, 500u);
    CHECK_EQ(passengers[0].seatId, 2u);
    CHECK_EQ(passengers[0].passengerEntry, 9002u);
    CHECK_EQ(passengers[1].seatId, 0u);
    CHECK_EQ(passengers[2].seatId, 1u);
    CHECK_EQ(store.GetCount(), 3u);
}

TEST(VehicleAccessoryStore_VehiclesStayIndependent)
{
    VehicleAccessoryStore store;
    store.Add(MakeAccessory(10, 0, 100));
    store.Add(MakeAccessory(11, 0, 110));
    store.Add(MakeAccessory(10, 1, 101));

    std::vector<VehicleAccessory> first = Passengers(store, 10);
    std::vector<VehicleAccessory> second = Passengers(store, 11);
    REQUIRE(first.size() == 2);
    REQUIRE(second.size() == 1);
    CHECK_EQ(first[1].passengerEntry, 101u);
    CHECK_EQ(second[0].passengerEntry, 110u);
}

TEST(VehicleAccessoryStore_EraseForgetsEveryPassengerOfTheVehicle)
{
    VehicleAccessoryStore store;
    store.Add(MakeAccessory(10, 0, 100));
    store.Add(MakeAccessory(10, 1, 101));
    store.Add(MakeAccessory(11, 0, 110));

    store.Erase(10);
    CHECK(Passengers(store, 10).empty());
    CHECK_EQ(Passengers(store, 11).size(), size_t(1));
    CHECK_EQ(store.GetCount(), 1u);

    store.Erase(10);
    CHECK_EQ(store.GetCount(), 1u);
}

TEST(VehicleAccessoryStore_ClearForgetsEveryVehicle)
{
    VehicleAccessoryStore store;
    store.Add(MakeAccessory(10, 0, 100));
    store.Add(MakeAccessory(11, 0, 110));
    store.Clear();

    CHECK(Passengers(store, 10).empty());
    CHECK(Passengers(store, 11).empty());
    CHECK_EQ(store.GetCount(), 0u);
}
