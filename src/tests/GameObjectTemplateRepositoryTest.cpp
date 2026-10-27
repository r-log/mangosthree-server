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

/// GameObjectTemplateRepository's read on the global WorldDatabase backed by fakes: the statement
/// names its columns, every column of a canned row lands in its member (a NULL text as ""; the first
/// row sets the result's width, so the short second row's texts read as NULL), and no row answers an
/// empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/gameobjects/GameObjectTemplateRepository.h"

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

// Each column of a `gameobject_template` row lands in its member, and the rows come back in order.
TEST(GameObjectTemplateRepository_GameObjectTemplates)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"1000", "1001", "1002", "a name", "an icon", "a caption", "an unk1", "1007", "1008", "1.5", "1010", "1011", "1012", "1013", "1014", "1015", "1016", "1017", "1018", "1019", "1020", "1021", "1022", "1023", "1024", "1025", "1026", "1027", "1028", "1029", "1030", "1031", "1032", "1033", "1034", "1035", "1036", "1037", "1038", "1039", "1040", "1041", "1042", "1043", "1044", "1045", "1046", "1047", "1048", "1049", "1050"},
        FakeRow{"2000", "0", "0"},
    });
    TableRead<GameObjectTemplateRow> read = GameObjectTemplateRepository::QueryGameObjectTemplates();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `faction`, `flags`, `size`, `questItem1`, `questItem2`, `questItem3`, `questItem4`, `questItem5`, `questItem6`, `data0`, `data1`, `data2`, `data3`, `data4`, `data5`, `data6`, `data7`, `data8`, `data9`, `data10`, `data11`, `data12`, `data13`, `data14`, `data15`, `data16`, `data17`, `data18`, `data19`, `data20`, `data21`, `data22`, `data23`, `data24`, `data25`, `data26`, `data27`, `data28`, `data29`, `data30`, `data31`, `unk2`, `mingold`, `maxgold` FROM `gameobject_template`");
    REQUIRE(read.rows.size() == 2);
    GameObjectTemplateRow const& row = read.rows[0];
    CHECK_EQ(row.entry, 1000u);
    CHECK_EQ(row.type, 1001u);
    CHECK_EQ(row.displayId, 1002u);
    CHECK_STR(row.name, "a name");
    CHECK_STR(row.iconName, "an icon");
    CHECK_STR(row.castBarCaption, "a caption");
    CHECK_STR(row.unk1, "an unk1");
    CHECK_EQ(row.faction, 1007u);
    CHECK_EQ(row.flags, 1008u);
    CHECK(row.size == 1.5f);
    for (uint32 i = 0; i < GAMEOBJECT_TEMPLATE_QUEST_ITEM_COLUMNS; ++i)
    {
        CHECK_EQ(row.questItems[i], 1010u + i);
    }
    for (uint32 i = 0; i < GAMEOBJECT_TEMPLATE_DATA_COLUMNS; ++i)
    {
        CHECK_EQ(row.data[i], 1016u + i);
    }
    CHECK_EQ(row.unk2, 1048u);
    CHECK_EQ(row.minGold, 1049u);
    CHECK_EQ(row.maxGold, 1050u);
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_STR(read.rows[1].name, "");
    CHECK_STR(read.rows[1].unk1, "");
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(GameObjectTemplateRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<GameObjectTemplateRow> read = GameObjectTemplateRepository::QueryGameObjectTemplates();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `entry`, `type`, `displayId`, `name`, `IconName`, `castBarCaption`, `unk1`, `faction`, `flags`, `size`, `questItem1`, `questItem2`, `questItem3`, `questItem4`, `questItem5`, `questItem6`, `data0`, `data1`, `data2`, `data3`, `data4`, `data5`, `data6`, `data7`, `data8`, `data9`, `data10`, `data11`, `data12`, `data13`, `data14`, `data15`, `data16`, `data17`, `data18`, `data19`, `data20`, `data21`, `data22`, `data23`, `data24`, `data25`, `data26`, `data27`, `data28`, `data29`, `data30`, `data31`, `unk2`, `mingold`, `maxgold` FROM `gameobject_template`) AS probe");
    }
    {
        Db db;
        TableRead<GameObjectTemplateRow> read = GameObjectTemplateRepository::QueryGameObjectTemplates();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
