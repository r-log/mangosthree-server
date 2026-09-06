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

// Every golden capture under src/tests/goldens/movement replays clean through
// the registry: decoded whole, re-encoded byte for byte. The files are listed
// here by name on purpose -- a golden that goes missing must fail, not vanish.
#include "TestHarness.h"

#include "Replay.hpp"
#include "Opcodes.h"

#include <fstream>
#include <string>

#ifndef MANGOS_TEST_DATA_DIR
#error "MANGOS_TEST_DATA_DIR must name src/tests/goldens (see src/tests/CMakeLists.txt)"
#endif

namespace
{
    // void, not value-returning: the harness's REQUIRE leaves the enclosing
    // function with a bare `return;` on failure (the P0-A plan hit exactly this).
    void ReplayGolden(const char* name, loadtest::ReplayReport& out)
    {
        const std::string path = std::string(MANGOS_TEST_DATA_DIR) + "/movement/" + name;
        std::ifstream capture(path.c_str());
        REQUIRE(bool(capture));
        out = loadtest::Replay(capture);
    }
}

TEST(GoldenCapture_peer_walk_replays_clean)
{
    loadtest::ReplayReport report;
    ReplayGolden("peer-walk-15595.log", report);
    CHECK(report.lines >= 60);
    CHECK_EQ(report.malformed, uint32(0));
    CHECK_EQ(report.failed, uint32(0));
    CHECK_EQ(report.exact, report.decoded);
    CHECK(report.Clean());
    // What a walk-and-relay must contain, both directions.
    CHECK(report.byOpcode.count(CMSG_MOVE_START_FORWARD) == 1);
    CHECK(report.byOpcode.count(MSG_MOVE_HEARTBEAT) == 1);
    CHECK(report.byOpcode.count(CMSG_MOVE_STOP) == 1);
    CHECK(report.byOpcode.count(SMSG_PLAYER_MOVE) == 1);
    CHECK(report.byOpcode.count(SMSG_MOVE_SET_RUN_SPEED) == 1);
    CHECK(report.byOpcode.count(CMSG_FORCE_RUN_SPEED_CHANGE_ACK) == 1);
}
