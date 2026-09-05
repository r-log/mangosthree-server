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

// The 4.3.4 movement-status codec in proto/wire. Everything here is pure: bytes
// in, a value out, and back. The legacy reader lives in src/game (Unit.cpp) and
// stays live until P2; these tests pin the replacement before it goes anywhere
// near the server.

#include "TestHarness.h"

#include "wire/MovementStatus.h"
#include "wire/MovementElements.h"

TEST(MovementStatus_defaults_are_a_plain_standing_mover)
{
    Wire::MovementStatus s;
    CHECK_EQ(s.guid, uint64(0));
    CHECK_EQ(s.flags, uint32(0));
    CHECK_EQ(s.flags2, uint32(0));
    CHECK_EQ(s.time, uint32(0));
    CHECK_EQ(s.counter, uint32(0));
    CHECK(s.has.orientation);
    CHECK(s.has.timestamp);
    CHECK(!s.has.pitch);
    CHECK(!s.has.spline);
    CHECK(!s.has.splineElevation);
    CHECK(!s.fall.present);
    CHECK(!s.transport.present);
    CHECK_EQ(int(s.transport.seat), -1);

    Wire::MovementStatus t;
    CHECK(s == t);
    t.pos.x = 1.0f;
    CHECK(!(s == t));

    // The vocabulary is closed by End; the count is what sequence tables size by.
    CHECK(int(Wire::Element::End) > int(Wire::Element::MovementCounter));
}
