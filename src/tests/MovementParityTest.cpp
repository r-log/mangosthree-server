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

// The one comparison the server's shadow mode and the replay tool both rest on:
// which field two statuses first differ in, by name, so a report can say it.
#include "TestHarness.h"

#include "wire/MovementParity.h"
#include "wire/MovementStatus.h"

TEST(MovementParity_equal_statuses_have_no_difference)
{
    Wire::MovementStatus a;
    a.guid = 0x42;
    a.pos.x = 1.5f;
    a.fall.present = true;
    a.fall.time = 300;
    Wire::MovementStatus b = a;
    CHECK(Wire::FirstDifference(a, b) == nullptr);
}

TEST(MovementParity_names_the_first_differing_field_in_struct_order)
{
    Wire::MovementStatus a;
    Wire::MovementStatus b;
    b.pos.z = 9.0f;
    b.fall.time = 7;                 // later in the order than pos.z
    CHECK_STR(Wire::FirstDifference(a, b), "pos.z");

    b.pos.z = 0.0f;
    CHECK_STR(Wire::FirstDifference(a, b), "fall.time");

    b.fall.time = 0;
    b.transport.hasVehicleId = true;
    CHECK_STR(Wire::FirstDifference(a, b), "transport.hasVehicleId");

    b.transport.hasVehicleId = false;
    b.has.heightChangeFailed = true;
    CHECK_STR(Wire::FirstDifference(a, b), "has.heightChangeFailed");
}

TEST(MovementParity_a_crossed_fall_angle_pair_is_named_as_the_cos_field)
{
    // The legacy header and the registry label the two fall-direction floats the
    // other way round; the server's shadow tells that case apart by asking for the
    // first difference and then checking the pair crossed. This pins the name it
    // will see.
    Wire::MovementStatus a;
    a.fall.present = true;
    a.fall.hasDirection = true;
    a.fall.cosAngle = 1.0f;
    a.fall.sinAngle = 0.0f;
    Wire::MovementStatus b = a;
    b.fall.cosAngle = 0.0f;
    b.fall.sinAngle = 1.0f;
    CHECK_STR(Wire::FirstDifference(a, b), "fall.cosAngle");
}

TEST(MovementParity_fall_time_is_named_before_the_transport_block_and_the_fall_angles)
{
    // The shadow's vehicle-id bin rests on this ordering: on seeing fall.time
    // named it patches that one field and re-compares the rest before crediting
    // the quirk, which is only sound while fall.time is the FIRST difference a
    // packet of this shape can produce. If a transport field or a fall angle
    // came first, the bin would credit the wrong packets and hide a real
    // disagreement behind a known one.
    Wire::MovementStatus a;
    a.fall.present = true;
    a.fall.hasDirection = true;
    a.transport.present = true;

    Wire::MovementStatus b = a;
    b.fall.time = 4242;                  // what the legacy reader wrote there
    b.transport.vehicleId = 4242;        // and where it belonged
    CHECK_STR(Wire::FirstDifference(a, b), "fall.time");

    Wire::MovementStatus c = a;
    c.fall.time = 4242;
    c.fall.cosAngle = 0.5f;
    CHECK_STR(Wire::FirstDifference(a, c), "fall.time");
}
