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

/// The DBC locale mask: which locales a session can read DBC text in.

#include "TestHarness.h"
#include "DBCStores.h"
#include "data/GameConfig.h"

// With no locale subdirectory loaded, only the default locale is in the mask.
TEST(DbcLocaleMask_NoSubdirectoriesLeaveTheDefault)
{
    CHECK_EQ(DbcLocaleMask(LOCALE_enUS, 0), uint32(1 << LOCALE_enUS));
    CHECK_EQ(DbcLocaleMask(LOCALE_deDE, 0), uint32(1 << LOCALE_deDE));
}

// The subdirectory bits are positions in fullLocaleNameList: enGB sits beside enUS, so every later name is one place on.
TEST(DbcLocaleMask_SubdirectoryBitsMapToTheirLocales)
{
    uint32 const enGB = 1 << 1;
    uint32 const deDE = 1 << 4;
    uint32 const itIT = 1 << 12;
    CHECK_EQ(DbcLocaleMask(LOCALE_enUS, deDE), uint32((1 << LOCALE_enUS) | (1 << LOCALE_deDE)));
    CHECK_EQ(DbcLocaleMask(LOCALE_frFR, enGB | itIT), uint32((1 << LOCALE_frFR) | (1 << LOCALE_enUS) | (1 << LOCALE_itIT)));
}

// A session reads DBC text in its own locale when that locale loaded, and in the default otherwise.
TEST(DbcLocaleMask_SessionsReadTheirOwnLocaleWhenLoaded)
{
    sGameConfig.SetDefaultDbcLocale(LOCALE_enUS);
    sGameConfig.SetAvailableDbcLocaleMask(DbcLocaleMask(LOCALE_enUS, 1 << 4));
    CHECK_EQ(int(sGameConfig.GetAvailableDbcLocale(LOCALE_enUS)), int(LOCALE_enUS));
    CHECK_EQ(int(sGameConfig.GetAvailableDbcLocale(LOCALE_deDE)), int(LOCALE_deDE));
    CHECK_EQ(int(sGameConfig.GetAvailableDbcLocale(LOCALE_frFR)), int(LOCALE_enUS));
    sGameConfig.SetAvailableDbcLocaleMask(0);
}
