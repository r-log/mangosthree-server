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

/// The hotfixes, added and read with no database: the rows in the order added, and a cleared store.

#include "TestHarness.h"
#include "data/HotfixStore.h"

namespace
{
    HotfixInfo MakeHotfix(uint32 type, uint32 entry, uint32 timestamp)
    {
        HotfixInfo info;
        info.Type = type;
        info.Entry = entry;
        info.Timestamp = timestamp;
        return info;
    }
}

TEST(HotfixStore_TheRowsAreHeldInTheOrderAdded)
{
    HotfixStore store;
    CHECK(store.All().empty());

    store.Add(MakeHotfix(1344965063, 50000, 1321000000));
    store.Add(MakeHotfix(1344965063, 50000, 1322000000));

    REQUIRE(store.All().size() == size_t(2));
    CHECK_EQ(store.All()[0].Timestamp, uint32(1321000000));
    CHECK_EQ(store.All()[1].Timestamp, uint32(1322000000));
    CHECK_EQ(store.All()[1].Entry, uint32(50000));
}

TEST(HotfixStore_ClearingForgetsEveryRow)
{
    HotfixStore store;
    store.Add(MakeHotfix(1, 2, 3));
    store.Clear();
    CHECK(store.All().empty());
}
