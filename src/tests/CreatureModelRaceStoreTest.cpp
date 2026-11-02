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

/// The racial models, added and looked up with no database: a native model's entries in the order
/// added, a model with none, and a cleared store.

#include "TestHarness.h"
#include "data/CreatureModelRaceStore.h"

namespace
{
    CreatureModelRace MakeRace(uint32 modelId, uint32 raceMask, uint32 racialModel)
    {
        CreatureModelRace race = CreatureModelRace();
        race.modelid = modelId;
        race.racemask = raceMask;
        race.modelid_racial = racialModel;
        return race;
    }
}

TEST(CreatureModelRaceStore_AModelHoldsItsEntriesInTheOrderAdded)
{
    CreatureModelRaceStore store;
    store.Add(MakeRace(2410, 0x1, 14335));
    store.Add(MakeRace(2408, 0x2, 14333));
    store.Add(MakeRace(2410, 0x4, 14336));

    CreatureModelRaceStore::CreatureModelRaceMapBounds model = store.ForModel(2410);
    REQUIRE(model.first != model.second);
    CHECK_EQ(model.first->second.racemask, uint32(0x1));
    CHECK_EQ(model.first->second.modelid_racial, uint32(14335));
    ++model.first;
    REQUIRE(model.first != model.second);
    CHECK_EQ(model.first->second.racemask, uint32(0x4));
    ++model.first;
    CHECK(model.first == model.second);

    CreatureModelRaceStore::CreatureModelRaceMapBounds none = store.ForModel(9999);
    CHECK(none.first == none.second);
}

TEST(CreatureModelRaceStore_ClearingForgetsEveryModel)
{
    CreatureModelRaceStore store;
    store.Add(MakeRace(2410, 0x1, 14335));
    store.Clear();

    CreatureModelRaceStore::CreatureModelRaceMapBounds model = store.ForModel(2410);
    CHECK(model.first == model.second);
}
