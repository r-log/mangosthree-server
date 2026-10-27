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

/// CreatureAddonRepository's reads on the global WorldDatabase backed by fakes: each statement names
/// its columns, every column of a canned row lands in its member (a NULL aura text as ""), and no
/// row answers an empty table or an unreadable one. ObjectMgr::LoadCreatureAddons fills both stores
/// and runs its checks; with no DBC loaded here, every mount, emote and aura spell is unknown.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "persistence/world/creatures/CreatureAddonRepository.h"

#include <vector>

namespace
{
    /// The fakes on the global world database.
    struct Db
    {
        FakeConnection query;
        FakeConnection async;
        SqlResultQueue results;
        AttachedFakes attached;

        Db() : query(WorldDatabase), async(WorldDatabase), attached(WorldDatabase, &query, &async, &results)
        {
        }
    };
}

// Each column of a `creature_addon` row lands in its member, and the rows come back in order.
TEST(CreatureAddonRepository_CreatureAddons)
{
    Db db;
    db.query.Answer("SELECT `guid`", FakeRows{
        FakeRow{"1000", "1001", "1002", "3", "4", "1005", "1006", "s1.7"},
        FakeRow{"2000", "2001", "2002", "1", "2", "2005", "2006"},
    });
    TableRead<CreatureAddonRow> read = CreatureAddonRepository::QueryCreatureAddons();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `guid`, `mount`, `bytes1`, `b2_0_sheath`, `b2_1_pvp_state`, `emote`, `moveflags`, `auras` FROM `creature_addon`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].guidOrEntry, 1000u);
    CHECK_EQ(read.rows[0].mount, 1001u);
    CHECK_EQ(read.rows[0].bytes1, 1002u);
    CHECK_EQ(uint32(read.rows[0].sheathState), 3u);
    CHECK_EQ(uint32(read.rows[0].pvpState), 4u);
    CHECK_EQ(read.rows[0].emote, 1005u);
    CHECK_EQ(read.rows[0].moveFlags, 1006u);
    CHECK_STR(read.rows[0].auras, "s1.7");
    CHECK_EQ(read.rows[1].guidOrEntry, 2000u);
    CHECK_STR(read.rows[1].auras, "");
}

// The template addons' statement names its columns, keyed by entry.
TEST(CreatureAddonRepository_CreatureTemplateAddons)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"30", "0", "0", "1", "0", "0", "0", "8876"},
    });
    TableRead<CreatureAddonRow> read = CreatureAddonRepository::QueryCreatureTemplateAddons();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `mount`, `bytes1`, `b2_0_sheath`, `b2_1_pvp_state`, `emote`, `moveflags`, `auras` FROM `creature_template_addon`");
    REQUIRE(read.rows.size() == 1);
    CHECK_EQ(read.rows[0].guidOrEntry, 30u);
    CHECK_STR(read.rows[0].auras, "8876");
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(CreatureAddonRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<CreatureAddonRow> read = CreatureAddonRepository::QueryCreatureAddons();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `guid`, `mount`, `bytes1`, `b2_0_sheath`, `b2_1_pvp_state`, `emote`, `moveflags`, `auras` FROM `creature_addon`) AS probe");
    }
    {
        Db db;
        TableRead<CreatureAddonRow> read = CreatureAddonRepository::QueryCreatureTemplateAddons();
        CHECK(read.status == TableReadStatus::QueryFailed);
    }
}

// The load fills both stores and checks them: an unknown mount and emote are set to 0, an empty aura
// text leaves no list, and a text whose spells are all unknown leaves a list of the terminator alone.
TEST(CreatureAddonLoad_FillsAndChecksBothTables)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"700001", "2410", "0", "1", "0", "69", "0", "8876 18950"},
        FakeRow{"700002", "0", "0", "1", "0", "0", "0", ""},
    });
    db.query.Answer("SELECT `guid`", FakeRows{
        FakeRow{"800001", "0", "0", "1", "0", "0", "0", ""},
    });
    sObjectMgr.LoadCreatureAddons();

    CreatureDataAddon const* first = ObjectMgr::GetCreatureTemplateAddon(700001);
    REQUIRE(first != NULL);
    CHECK_EQ(first->mount, 0u);
    CHECK_EQ(first->emote, 0u);
    REQUIRE(first->auras != NULL);
    CHECK_EQ(first->auras[0], 0u);

    CreatureDataAddon const* second = ObjectMgr::GetCreatureTemplateAddon(700002);
    REQUIRE(second != NULL);
    CHECK(second->auras == NULL);

    CreatureDataAddon const* spawn = ObjectMgr::GetCreatureAddon(800001);
    REQUIRE(spawn != NULL);
    CHECK(spawn->auras == NULL);
}
