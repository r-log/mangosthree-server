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

/// CreatureTemplateSpellsRepository's read on the global WorldDatabase backed by fakes: the
/// statement names its columns, every column of a canned row lands in its member, and no row
/// answers an empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/creatures/CreatureTemplateSpellsRepository.h"

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

// Each column of a `creature_template_spells` row lands in its member, and the rows come back in order.
TEST(CreatureTemplateSpellsRepository_CreatureTemplateSpells)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007", "1008", "1009", "1010"},
        FakeRow{"2000", "2001", "0", "0", "0", "0", "0", "0", "0", "0", "2010"},
    });
    TableRead<CreatureTemplateSpellsRow> read = CreatureTemplateSpellsRepository::QueryCreatureTemplateSpells();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `spell1`, `spell2`, `spell3`, `spell4`, `spell5`, `spell6`, `spell7`, `spell8`, `spell9`, `spell10` FROM `creature_template_spells`");
    REQUIRE(read.rows.size() == 2);
    CHECK_EQ(read.rows[0].entry, 1000u);
    for (uint32 i = 0; i < CREATURE_TEMPLATE_SPELL_COLUMNS; ++i)
    {
        CHECK_EQ(read.rows[0].spells[i], 1001u + i);
    }
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_EQ(read.rows[1].spells[0], 2001u);
    CHECK_EQ(read.rows[1].spells[1], 0u);
    CHECK_EQ(read.rows[1].spells[9], 2010u);
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(CreatureTemplateSpellsRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<CreatureTemplateSpellsRow> read = CreatureTemplateSpellsRepository::QueryCreatureTemplateSpells();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `entry`, `spell1`, `spell2`, `spell3`, `spell4`, `spell5`, `spell6`, `spell7`, `spell8`, `spell9`, `spell10` FROM `creature_template_spells`) AS probe");
    }
    {
        Db db;
        TableRead<CreatureTemplateSpellsRow> read = CreatureTemplateSpellsRepository::QueryCreatureTemplateSpells();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
