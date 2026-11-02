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

/// The item conversions, set and looked up with no database: an item with no target, an item with
/// one, a second target for the same item, and each part cleared on its own.

#include "TestHarness.h"
#include "data/ItemConvertStore.h"

TEST(ItemConvertStore_AnItemWithNoTargetAnswersZero)
{
    ItemConvertStore store;
    CHECK_EQ(store.RaceConvert(18606), uint32(0));
    CHECK_EQ(store.ExpireConvert(18606), uint32(0));
}

TEST(ItemConvertStore_AnItemAnswersItsTargetInEachPart)
{
    ItemConvertStore store;
    store.SetRaceConvert(18606, 18607);
    store.SetExpireConvert(39878, 39883);

    CHECK_EQ(store.RaceConvert(18606), uint32(18607));
    CHECK_EQ(store.ExpireConvert(18606), uint32(0));
    CHECK_EQ(store.ExpireConvert(39878), uint32(39883));
    CHECK_EQ(store.RaceConvert(39878), uint32(0));

    store.SetRaceConvert(18606, 18608);
    CHECK_EQ(store.RaceConvert(18606), uint32(18608));
}

TEST(ItemConvertStore_EachPartClearsOnItsOwn)
{
    ItemConvertStore store;
    store.SetRaceConvert(18606, 18607);
    store.SetExpireConvert(39878, 39883);

    store.ClearRaceConverts();
    CHECK_EQ(store.RaceConvert(18606), uint32(0));
    CHECK_EQ(store.ExpireConvert(39878), uint32(39883));

    store.ClearExpireConverts();
    CHECK_EQ(store.ExpireConvert(39878), uint32(0));
}
