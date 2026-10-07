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

/// The order the area-target bodies sort their targets by (TargetDistanceOrderFarAway): the furthest
/// from the main target first, and a target outside the main target's frame before every placed one.
/// The distances are distinct, because the order holds two equal ones each before the other.

#include "TestHarness.h"
#include "WorldHandlers/SpellTargetDistanceOrder.h"
#include "Creature.h"

#include <list>
#include <vector>

namespace
{
    /// Places the creature in map 0's world frame at (x, 0, 0).
    void PlaceAt(Creature& creature, float x)
    {
        creature.Place().EnterFrame(Geometry::Frame::World(0, 0), Geometry::Vector3(x, 0.0f, 0.0f), 0.0f);
    }
}

TEST(SpellTargetDistanceOrder_FarAwaySortsTheFurthestFromTheMainTargetFirst)
{
    Creature mainTarget(CREATURE_SUBTYPE_GENERIC);
    Creature at5(CREATURE_SUBTYPE_GENERIC);
    Creature at10(CREATURE_SUBTYPE_GENERIC);
    Creature at20(CREATURE_SUBTYPE_GENERIC);
    Creature unplaced(CREATURE_SUBTYPE_GENERIC);
    PlaceAt(mainTarget, 0.0f);
    PlaceAt(at5, 5.0f);
    PlaceAt(at10, 10.0f);
    PlaceAt(at20, 20.0f);

    std::list<Unit*> targets;
    targets.push_back(&at10);
    targets.push_back(&at5);
    targets.push_back(&at20);
    targets.sort(TargetDistanceOrderFarAway(&mainTarget));
    std::vector<Unit*> sorted(targets.begin(), targets.end());
    REQUIRE(sorted.size() == 3);
    CHECK(sorted[0] == &at20);
    CHECK(sorted[1] == &at10);
    CHECK(sorted[2] == &at5);

    targets.resize(2);
    REQUIRE(targets.size() == 2);
    CHECK(targets.front() == &at20);
    CHECK(targets.back() == &at10);

    std::list<Unit*> withUnplaced;
    withUnplaced.push_back(&at10);
    withUnplaced.push_back(&unplaced);
    withUnplaced.push_back(&at5);
    withUnplaced.push_back(&at20);
    withUnplaced.sort(TargetDistanceOrderFarAway(&mainTarget));
    std::vector<Unit*> unplacedFirst(withUnplaced.begin(), withUnplaced.end());
    REQUIRE(unplacedFirst.size() == 4);
    CHECK(unplacedFirst[0] == &unplaced);
    CHECK(unplacedFirst[1] == &at20);
    CHECK(unplacedFirst[2] == &at10);
    CHECK(unplacedFirst[3] == &at5);
}
