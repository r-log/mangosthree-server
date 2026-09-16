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

// The kernel's native behaviours (P5-B family 1): pure, driven with scripted Sights.

#include "TestHarness.h"
#include "BehaviourModel.h"

using namespace Motion;

TEST(MotionBehaviour_IntentValuesAreKernelSafe)
{
    MoveIntent m = MoveIntent::Move(Vector3(1.0f, 2.0f, 3.0f), MOVE_WALK).AtSpeed(24.0f);
    CHECK(m.act == MoveIntent::Act::Move);
    CHECK(m.Has(MOVE_WALK));
    CHECK_EQ(m.speed, 24.0f);
    EffectLaunch l;
    l.kind = EffectLaunch::Jump;
    l.point = Vector3(4.0f, 5.0f, 6.0f);
    MoveIntent j = MoveIntent::Launch(l);
    CHECK(j.act == MoveIntent::Act::Launch);
    CHECK_EQ(j.launch.point.y, 5.0f);
    Facing f = Facing::ToTarget(0x1234ull);
    CHECK(f.mode == Facing::Mode::Target);
    CHECK_EQ(f.target, uint64(0x1234ull));
}
