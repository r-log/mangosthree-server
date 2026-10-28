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

/// Whether a passenger may step aboard the vessel his movement names: she must sail his map, and
/// he must be within her reach. A modified client can name any vessel in the world.

#include "TestHarness.h"
#include "Transports.h"

namespace
{
    Geometry::Placement VesselAt(float x, float y, float z)
    {
        Geometry::Placement pose;
        pose.MoveTo(x, y, z);
        return pose;
    }
}

TEST(TransportBoarding_AVesselOnHisMapWithinReachIsBoarded)
{
    Geometry::Placement const pose = VesselAt(100.0f, 200.0f, 10.0f);
    CHECK(Transport::CanStepAboard(1, Geometry::Vector3(130.0f, 200.0f, 10.0f), 1, pose, 40.0f));
}

TEST(TransportBoarding_AVesselSailingAnotherMapIsRefused)
{
    Geometry::Placement const pose = VesselAt(100.0f, 200.0f, 10.0f);
    CHECK(!Transport::CanStepAboard(1, Geometry::Vector3(100.0f, 200.0f, 10.0f), 0, pose, 40.0f));
}

TEST(TransportBoarding_AVesselOutOfReachOnHisMapIsRefused)
{
    Geometry::Placement const pose = VesselAt(100.0f, 200.0f, 10.0f);
    CHECK(!Transport::CanStepAboard(1, Geometry::Vector3(100.0f, 260.0f, 10.0f), 1, pose, 40.0f));
    CHECK(!Transport::CanStepAboard(1, Geometry::Vector3(100.0f, 200.0f, 60.0f), 1, pose, 40.0f));
}
