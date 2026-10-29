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

/// A creature's swim state is one fact: SetSwim writes the movement word and tells the kernel
/// together, so a creature that spawns in water and later leaves it sends the stop-swim the
/// client expects, and one that changes its entry in water sends the start-swim.
#include "TestHarness.h"
#include "Creature.h"

namespace
{
    /// A Creature with its update fields allocated, never in a world.
    class SwimProbe : public Creature
    {
        public:
            SwimProbe() : Creature(CREATURE_SUBTYPE_GENERIC)
            {
                _InitValues();
            }

            /// Unit::CleanupsBeforeDelete runs the in-world teardown only for an object whose
            /// update fields exist; this one was never in a world, so they go first.
            ~SwimProbe()
            {
                delete[] m_uint32Values;
                m_uint32Values = NULL;
            }
    };
}

TEST(Creature_SetSwim_tells_the_kernel)
{
    SwimProbe probe;
    Unit const& unit = probe;
    CHECK(!probe.IsSwimming());
    CHECK(!unit.MotionState().Desired().swim);

    probe.SetSwim(true);
    CHECK(probe.IsSwimming());
    CHECK(unit.MotionState().Desired().swim);
    CHECK(unit.MotionState().Confirmed().swim);

    probe.SetSwim(false);
    CHECK(!probe.IsSwimming());
    CHECK(!unit.MotionState().Desired().swim);
    CHECK(!unit.MotionState().Confirmed().swim);
}
