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

/// GameObjectAddonRepository's read on the global WorldDatabase backed by fakes: the statement names
/// its columns, every column of a canned row lands in its member, and no row answers an empty table
/// or an unreadable one, as the count of the same select says. ObjectMgr::LoadGameObjectAddon fills
/// the store and runs its checks.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "ObjectMgr.h"
#include "persistence/world/spawns/GameObjectAddonRepository.h"

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

// Each column of a `gameobject_addon` row lands in its member, and the rows come back in order.
TEST(GameObjectAddonRepository_GameObjectAddons)
{
    Db db;
    db.query.Answer("SELECT `guid`", FakeRows{
        FakeRow{"1000", "0.25", "-0.5", "0.75", "1"},
        FakeRow{"2000", "0", "0", "0", "1"},
    });
    TableRead<GameObjectAddonRow> read = GameObjectAddonRepository::QueryGameObjectAddons();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `guid`, `path_rotation0`, `path_rotation1`, `path_rotation2`, `path_rotation3` FROM `gameobject_addon`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].guid, 1000u);
    CHECK(read.rows[0].pathRotation0 == 0.25f);
    CHECK(read.rows[0].pathRotation1 == -0.5f);
    CHECK(read.rows[0].pathRotation2 == 0.75f);
    CHECK(read.rows[0].pathRotation3 == 1.0f);
    CHECK_EQ(read.rows[1].guid, 2000u);
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(GameObjectAddonRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<GameObjectAddonRow> read = GameObjectAddonRepository::QueryGameObjectAddons();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `guid`, `path_rotation0`, `path_rotation1`, `path_rotation2`, `path_rotation3` FROM `gameobject_addon`) AS probe");
    }
    {
        Db db;
        TableRead<GameObjectAddonRow> read = GameObjectAddonRepository::QueryGameObjectAddons();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}

// The load fills the store and checks it: the path rotation of an existing game object that is not a
// unit quaternion becomes (0, 0, 0, 1), a unit one stays, and the addon of a game object that does
// not exist is kept as read.
TEST(GameObjectAddonLoad_FillsAndChecks)
{
    sObjectMgr.NewGOData(900001);
    sObjectMgr.NewGOData(900002);
    Db db;
    db.query.Answer("SELECT `guid`", FakeRows{
        FakeRow{"900001", "0.5", "0.5", "0.5", "0.5"},
        FakeRow{"900002", "0", "0", "2", "2"},
        FakeRow{"900003", "0", "0", "2", "2"},
    });
    sObjectMgr.LoadGameObjectAddon();

    GameObjectDataAddon const* unit = ObjectMgr::GetGameObjectAddon(900001);
    REQUIRE(unit != NULL);
    CHECK_EQ(unit->guid, 900001u);
    CHECK(unit->path_rotation.x == 0.5f);
    CHECK(unit->path_rotation.w == 0.5f);

    GameObjectDataAddon const* reset = ObjectMgr::GetGameObjectAddon(900002);
    REQUIRE(reset != NULL);
    CHECK(reset->path_rotation.x == 0.0f);
    CHECK(reset->path_rotation.y == 0.0f);
    CHECK(reset->path_rotation.z == 0.0f);
    CHECK(reset->path_rotation.w == 1.0f);

    GameObjectDataAddon const* orphan = ObjectMgr::GetGameObjectAddon(900003);
    REQUIRE(orphan != NULL);
    CHECK(orphan->path_rotation.z == 2.0f);

    CHECK(ObjectMgr::GetGameObjectAddon(900004) == NULL);

    sObjectMgr.DeleteGOData(900001);
    sObjectMgr.DeleteGOData(900002);
}
