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

/// The server configuration's 64-bit values come back whole through both getters.

#include "TestHarness.h"
#include "World.h"
#include "data/GameConfig.h"

// A start money above 32 bits keeps its high bits through World's getter and GameConfig's.
TEST(GameConfig_SixtyFourBitValuesKeepTheirHighBits)
{
    uint64 const previous = sWorld.getConfig(CONFIG_UINT64_START_PLAYER_MONEY);
    sWorld.setConfig(CONFIG_UINT64_START_PLAYER_MONEY, uint64(9999999999ull));
    CHECK_EQ(sWorld.getConfig(CONFIG_UINT64_START_PLAYER_MONEY), uint64(9999999999ull));
    CHECK_EQ(sGameConfig.getConfig(CONFIG_UINT64_START_PLAYER_MONEY), uint64(9999999999ull));
    sWorld.setConfig(CONFIG_UINT64_START_PLAYER_MONEY, previous);
}
