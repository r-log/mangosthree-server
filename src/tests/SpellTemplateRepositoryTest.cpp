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

/// SpellTemplateRepository's read on the global WorldDatabase backed by fakes: the statement names
/// its columns, every column of a canned row lands in its member (a NULL comment as ""; the first row
/// sets the result's width, so the short second row's comment reads as NULL), and no row answers an
/// empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/spells/SpellTemplateRepository.h"

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

// Each column of a `spell_template` row lands in its member, and the rows come back in order.
TEST(SpellTemplateRepository_SpellTemplates)
{
    Db db;
    FakeRow first{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007", "1008", "1009", "1010", "1011", "1012", "1013", "1014", "1015", "a spell"};
    FakeRow second{"2000", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"};
    db.query.Answer("SELECT `id`", FakeRows{first, second});
    TableRead<SpellTemplateRow> read = SpellTemplateRepository::QuerySpellTemplates();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `id`, `attr`, `attr_ex`, `attr_ex2`, `attr_ex3`, `proc_flags`, `proc_chance`, `duration_index`, `effect0`, `effect0_implicit_target_a`, `effect0_implicit_target_b`, `effect0_radius_idx`, `effect0_apply_aura_name`, `effect0_misc_value`, `effect0_misc_value_b`, `effect0_trigger_spell`, `comments` FROM `spell_template`");
    REQUIRE(read.rows.size() == 2);
    SpellTemplateRow const& row = read.rows[0];
    CHECK_EQ(row.id, 1000u);
    CHECK_EQ(row.attributes, 1001u);
    CHECK_EQ(row.attributesEx, 1002u);
    CHECK_EQ(row.attributesEx2, 1003u);
    CHECK_EQ(row.attributesEx3, 1004u);
    CHECK_EQ(row.procFlags, 1005u);
    CHECK_EQ(row.procChance, 1006u);
    CHECK_EQ(row.durationIndex, 1007u);
    CHECK_EQ(row.effect, 1008u);
    CHECK_EQ(row.effectImplicitTargetA, 1009u);
    CHECK_EQ(row.effectImplicitTargetB, 1010u);
    CHECK_EQ(row.effectRadiusIndex, 1011u);
    CHECK_EQ(row.effectApplyAuraName, 1012u);
    CHECK_EQ(row.effectMiscValue, 1013u);
    CHECK_EQ(row.effectMiscValueB, 1014u);
    CHECK_EQ(row.effectTriggerSpell, 1015u);
    CHECK_STR(row.comments, "a spell");
    CHECK_EQ(read.rows[1].id, 2000u);
    CHECK_STR(read.rows[1].comments, "");
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(SpellTemplateRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<SpellTemplateRow> read = SpellTemplateRepository::QuerySpellTemplates();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `id`, `attr`, `attr_ex`, `attr_ex2`, `attr_ex3`, `proc_flags`, `proc_chance`, `duration_index`, `effect0`, `effect0_implicit_target_a`, `effect0_implicit_target_b`, `effect0_radius_idx`, `effect0_apply_aura_name`, `effect0_misc_value`, `effect0_misc_value_b`, `effect0_trigger_spell`, `comments` FROM `spell_template`) AS probe");
    }
    {
        Db db;
        TableRead<SpellTemplateRow> read = SpellTemplateRepository::QuerySpellTemplates();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
