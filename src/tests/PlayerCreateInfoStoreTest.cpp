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

/// The player create info and the XP of each level, with no database: a race and class with no info,
/// an info the loader edits in place, its level array owned by the store, and the XP of a level, of a
/// level past the last and of a level reset.

#include "TestHarness.h"
#include "data/PlayerCreateInfoStore.h"

TEST(PlayerCreateInfoStore_ARaceAndClassStartEmpty)
{
    PlayerCreateInfoStore store;
    PlayerInfo const& info = store.Get(RACE_HUMAN, CLASS_WARRIOR);
    CHECK_EQ(info.displayId_m, uint16(0));
    CHECK_EQ(info.displayId_f, uint16(0));
    CHECK(info.levelInfo == NULL);
    CHECK(info.item.empty());
}

TEST(PlayerCreateInfoStore_TheLoaderEditsAnInfoInPlace)
{
    PlayerCreateInfoStore store;
    PlayerInfo& info = store.ForEdit(RACE_ORC, CLASS_SHAMAN);
    info.mapId = 1;
    info.displayId_m = 51;
    info.levelInfo = new PlayerLevelInfo[3];
    info.levelInfo[2].stats[STAT_STRENGTH] = 24;

    PlayerInfo const& found = store.Get(RACE_ORC, CLASS_SHAMAN);
    CHECK_EQ(found.mapId, uint32(1));
    CHECK_EQ(found.displayId_m, uint16(51));
    CHECK_EQ(found.levelInfo[2].stats[STAT_STRENGTH], uint8(24));
    CHECK_EQ(store.Get(RACE_ORC, CLASS_WARRIOR).displayId_m, uint16(0));
}

TEST(PlayerCreateInfoStore_ALevelAnswersItsXp)
{
    PlayerCreateInfoStore store;
    CHECK_EQ(store.XpForLevel(1), uint32(0));

    store.ResetXpForLevels(3);
    store.SetXpForLevel(1, 400);
    store.SetXpForLevel(2, 900);
    CHECK_EQ(store.XpForLevel(0), uint32(0));
    CHECK_EQ(store.XpForLevel(1), uint32(400));
    CHECK_EQ(store.XpForLevel(2), uint32(900));
    CHECK_EQ(store.XpForLevel(3), uint32(0));

    store.ResetXpForLevels(2);
    CHECK_EQ(store.XpForLevel(1), uint32(0));
    CHECK_EQ(store.XpForLevel(2), uint32(0));
}
