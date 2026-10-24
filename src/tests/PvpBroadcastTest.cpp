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

/// The battleground and outdoor PvP broadcasts: each manager hands what it is told to the callback the
/// world installs, once per call, the defense message with its zone and text, and the battleground
/// announcement with its text and every argument as given.

#include "TestHarness.h"
#include "OutdoorPvPMgr.h"
#include "BattleGroundMgr.h"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
    /// Formats the arguments the callback is handed with `format`, the way the world's text builder
    /// formats them with each locale's text: through a copy, so the arguments stay readable.
    std::string Formatted(char const* format, va_list* args)
    {
        va_list copy;
        va_copy(copy, *args);
        char text[256];
        vsnprintf(text, sizeof(text), format, copy);
        va_end(copy);
        return text;
    }
}

TEST(PvpBroadcast_DefenseMessageReachesTheInstalledCallback)
{
    OutdoorPvPMgr mgr;
    std::vector<std::pair<uint32, int32> > told;
    mgr.SetDefenseMessageSink([&told](uint32 zoneId, int32 textId)
    {
        told.push_back(std::make_pair(zoneId, textId));
    });

    mgr.SendDefenseMessage(ZONE_ID_ZANGARMARSH, 10055);
    mgr.SendDefenseMessage(ZONE_ID_NAGRAND, -7);

    REQUIRE(told.size() == size_t(2));
    CHECK(told[0] == std::make_pair(uint32(ZONE_ID_ZANGARMARSH), int32(10055)));
    CHECK(told[1] == std::make_pair(uint32(ZONE_ID_NAGRAND), int32(-7)));
}

TEST(PvpBroadcast_WorldTextHandsOnTheTextAndEveryArgument)
{
    BattleGroundMgr mgr;
    std::vector<std::string> told;
    mgr.SetWorldTextSink([&told](int32 textId, va_list* args)
    {
        told.push_back(std::to_string(textId) + ":" + Formatted("%s %u-%u %d", args) + "|" + Formatted("%s", args));
    });

    mgr.SendWorldText(712, "Warsong Gulch", 80u, 85u, -3);

    REQUIRE(told.size() == size_t(1));
    CHECK_STR(told[0], "712:Warsong Gulch 80-85 -3|Warsong Gulch");
}

TEST(PvpBroadcast_WorldTextWithoutArguments)
{
    BattleGroundMgr mgr;
    std::vector<int32> told;
    mgr.SetWorldTextSink([&told](int32 textId, va_list*)
    {
        told.push_back(textId);
    });

    mgr.SendWorldText(719);
    mgr.SendWorldText(720);

    CHECK(told == std::vector<int32>({ 719, 720 }));
}
