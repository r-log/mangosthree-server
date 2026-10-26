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

/// SpellDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/spells/SpellDataRepository.h"

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

// Each column of a row of a table of spell references the checks validate lands in its member, and
// the rows come back in order.
TEST(SpellDataRepository_UsedSpells)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "-1001", "4294967298", "1003", "-1004", "-1005", "-1006", "-1007", "-1008",
            "-1009", "s1.10", "s1.11"},
        FakeRow{"2000", "-2001", "8589934594", "2003", "-2004", "-2005", "-2006", "-2007", "-2008",
            "-2009", "s2.10", "s2.11"},
    });
    std::vector<UsedSpellRow> rows = SpellDataRepository::QueryUsedSpells("spell_check");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spellid, 1000u);
    CHECK_EQ(rows[0].spellFamilyName, -1001);
    CHECK_EQ(rows[0].spellFamilyMaskA, uint64(4294967298ull));
    CHECK_EQ(rows[0].spellFamilyMaskB, 1003u);
    CHECK_EQ(rows[0].spellIcon, -1004);
    CHECK_EQ(rows[0].spellVisual, -1005);
    CHECK_EQ(rows[0].spellCategory, -1006);
    CHECK_EQ(rows[0].effectType, -1007);
    CHECK_EQ(rows[0].effectAura, -1008);
    CHECK_EQ(rows[0].effectIdx, -1009);
    CHECK_STR(rows[0].name, "s1.10");
    CHECK_STR(rows[0].code, "s1.11");
    CHECK_EQ(rows[1].spellid, 2000u);
}

// Each column of a `spell_area` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_SpellAreas)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1", "1004", "10005", "-1006", "1007", "108", "1"},
        FakeRow{"2000", "2001", "2002", "1", "2004", "20005", "-2006", "2007", "208", "1"},
    });
    std::vector<SpellAreaRow> rows = SpellDataRepository::QuerySpellAreas();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spell, 1000u);
    CHECK_EQ(rows[0].area, 1001u);
    CHECK_EQ(rows[0].questStart, 1002u);
    CHECK_EQ(rows[0].questStartActive, true);
    CHECK_EQ(rows[0].questEnd, 1004u);
    CHECK_EQ(rows[0].conditionId, uint16(10005));
    CHECK_EQ(rows[0].auraSpell, -1006);
    CHECK_EQ(rows[0].racemask, 1007u);
    CHECK_EQ(rows[0].gender, uint8(108));
    CHECK_EQ(rows[0].autocast, true);
    CHECK_EQ(rows[1].spell, 2000u);
}

// Each column of a `spell_chain` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_SpellChains)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "103", "1004"},
        FakeRow{"2000", "2001", "2002", "203", "2004"},
    });
    std::vector<SpellChainRow> rows = SpellDataRepository::QuerySpellChains();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spellId, 1000u);
    CHECK_EQ(rows[0].prevSpell, 1001u);
    CHECK_EQ(rows[0].firstSpell, 1002u);
    CHECK_EQ(rows[0].rank, uint8(103));
    CHECK_EQ(rows[0].reqSpell, 1004u);
    CHECK_EQ(rows[1].spellId, 2000u);
}

// Each column of a `spell_learn_spell` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_LearnSpells)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1"},
        FakeRow{"2000", "2001", "1"},
    });
    std::vector<LearnSpellRow> rows = SpellDataRepository::QueryLearnSpells();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].spellID, 1001u);
    CHECK_EQ(rows[0].active, true);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `spell_target_position` row lands in its member, and the rows come back in
// order.
TEST(SpellDataRepository_TargetPositions)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002.5", "1003.5", "1004.5", "1005.5"},
        FakeRow{"2000", "2001", "2002.5", "2003.5", "2004.5", "2005.5"},
    });
    std::vector<SpellTargetPositionRow> rows = SpellDataRepository::QueryTargetPositions();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_EQ(rows[0].targetMap, 1001u);
    CHECK_EQ(rows[0].targetPositionX, 1002.5f);
    CHECK_EQ(rows[0].targetPositionY, 1003.5f);
    CHECK_EQ(rows[0].targetPositionZ, 1004.5f);
    CHECK_EQ(rows[0].targetOrientation, 1005.5f);
    CHECK_EQ(rows[1].id, 2000u);
}

// Each column of a `spell_bonus_data` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_Bonuses)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003.5", "1004.5"},
        FakeRow{"2000", "2001.5", "2002.5", "2003.5", "2004.5"},
    });
    std::vector<SpellBonusRow> rows = SpellDataRepository::QueryBonuses();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].directBonus, 1001.5f);
    CHECK_EQ(rows[0].dotBonus, 1002.5f);
    CHECK_EQ(rows[0].apBonus, 1003.5f);
    CHECK_EQ(rows[0].apDotBonus, 1004.5f);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `spell_elixir` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_Elixirs)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "101"},
        FakeRow{"2000", "201"},
    });
    std::vector<SpellElixirRow> rows = SpellDataRepository::QueryElixirs();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].mask, uint8(101));
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `spell_threat` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_Threats)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "10001", "1002.5", "1003.5"},
        FakeRow{"2000", "20001", "2002.5", "2003.5"},
    });
    std::vector<SpellThreatRow> rows = SpellDataRepository::QueryThreats();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].threat, uint16(10001));
    CHECK_EQ(rows[0].multiplier, 1002.5f);
    CHECK_EQ(rows[0].apBonus, 1003.5f);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `spell_pet_auras` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_PetAuras)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003"},
        FakeRow{"2000", "2001", "2002", "2003"},
    });
    std::vector<SpellPetAuraRow> rows = SpellDataRepository::QueryPetAuras();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].spell, 1000u);
    CHECK_EQ(rows[0].effectId, 1001u);
    CHECK_EQ(rows[0].pet, 1002u);
    CHECK_EQ(rows[0].aura, 1003u);
    CHECK_EQ(rows[1].spell, 2000u);
}

// Each column of a `spell_proc_event` row lands in its member, and the rows come back in order.
TEST(SpellDataRepository_ProcEvents)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005", "1006", "1007", "1008", "1009",
            "1010", "1011", "1012", "1013", "1014.5", "1015.5", "1016"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005", "2006", "2007", "2008", "2009",
            "2010", "2011", "2012", "2013", "2014.5", "2015.5", "2016"},
    });
    std::vector<SpellProcEventRow> rows = SpellDataRepository::QueryProcEvents();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].schoolMask, 1001u);
    CHECK_EQ(rows[0].spellFamilyName, 1002u);
    CHECK_EQ(rows[0].spellFamilyMaskA[0], 1003u);
    CHECK_EQ(rows[0].spellFamilyMaskA[1], 1004u);
    CHECK_EQ(rows[0].spellFamilyMaskA[2], 1005u);
    CHECK_EQ(rows[0].spellFamilyMaskB[0], 1006u);
    CHECK_EQ(rows[0].spellFamilyMaskB[1], 1007u);
    CHECK_EQ(rows[0].spellFamilyMaskB[2], 1008u);
    CHECK_EQ(rows[0].spellFamilyMaskC[0], 1009u);
    CHECK_EQ(rows[0].spellFamilyMaskC[1], 1010u);
    CHECK_EQ(rows[0].spellFamilyMaskC[2], 1011u);
    CHECK_EQ(rows[0].procFlags, 1012u);
    CHECK_EQ(rows[0].procEx, 1013u);
    CHECK_EQ(rows[0].ppmRate, 1014.5f);
    CHECK_EQ(rows[0].customChance, 1015.5f);
    CHECK_EQ(rows[0].cooldown, 1016u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `spell_proc_item_enchant` row lands in its member, and the rows come back in
// order.
TEST(SpellDataRepository_ProcItemEnchants)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5"},
        FakeRow{"2000", "2001.5"},
    });
    std::vector<SpellProcItemEnchantRow> rows = SpellDataRepository::QueryProcItemEnchants();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].ppmRate, 1001.5f);
    CHECK_EQ(rows[1].entry, 2000u);
}
