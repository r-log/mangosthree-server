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

/// NpcDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/npcs/NpcDataRepository.h"

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

// Each column of a `gossip_menu` row lands in its member, and the rows come back in order.
TEST(NpcDataRepository_GossipMenus)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "10003"},
        FakeRow{"2000", "2001", "2002", "20003"},
    });
    std::vector<GossipMenuRow> rows = NpcDataRepository::QueryGossipMenus();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].textId, 1001u);
    CHECK_EQ(rows[0].scriptId, 1002u);
    CHECK_EQ(rows[0].conditionId, uint16(10003));
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `gossip_menu_option` row lands in its member, and the rows come back in order.
TEST(NpcDataRepository_GossipMenuOptions)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "102", "s1.3", "1004", "1005", "-1006", "1007", "1008", "109",
            "1010", "s1.11", "10012"},
        FakeRow{"2000", "2001", "202", "s2.3", "2004", "2005", "-2006", "2007", "2008", "209",
            "2010", "s2.11", "20012"},
    });
    std::vector<GossipMenuOptionRow> rows = NpcDataRepository::QueryGossipMenuOptions();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].menuId, 1000u);
    CHECK_EQ(rows[0].id, 1001u);
    CHECK_EQ(rows[0].optionIcon, uint8(102));
    CHECK_STR(rows[0].optionText, "s1.3");
    CHECK_EQ(rows[0].optionId, 1004u);
    CHECK_EQ(rows[0].npcOptionNpcflag, 1005u);
    CHECK_EQ(rows[0].actionMenuId, -1006);
    CHECK_EQ(rows[0].actionPoiId, 1007u);
    CHECK_EQ(rows[0].actionScriptId, 1008u);
    CHECK_EQ(rows[0].boxCoded, uint8(109));
    CHECK_EQ(rows[0].boxMoney, 1010u);
    CHECK_STR(rows[0].boxText, "s1.11");
    CHECK_EQ(rows[0].conditionId, uint16(10012));
    CHECK_EQ(rows[1].menuId, 2000u);
}

// Each column of a row of a trainer spell table (`npc_trainer` or `npc_trainer_template`) lands in
// its member, and the rows come back in order.
TEST(NpcDataRepository_TrainerSpells)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "1004", "1005"},
        FakeRow{"2000", "2001", "2002", "2003", "2004", "2005"},
    });
    std::vector<TrainerSpellRow> rows = NpcDataRepository::QueryTrainerSpells("npc_trainer");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].spell, 1001u);
    CHECK_EQ(rows[0].spellcost, 1002u);
    CHECK_EQ(rows[0].reqskill, 1003u);
    CHECK_EQ(rows[0].reqskillvalue, 1004u);
    CHECK_EQ(rows[0].reqlevel, 1005u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a row of a vendor item table (`npc_vendor` or `npc_vendor_template`) lands in its
// member, and the rows come back in order.
TEST(NpcDataRepository_VendorItems)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "-1001", "1002", "1003", "1004", "10005"},
        FakeRow{"2000", "-2001", "2002", "2003", "2004", "20005"},
    });
    std::vector<VendorItemRow> rows = NpcDataRepository::QueryVendorItems("npc_vendor");
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].item, -1001);
    CHECK_EQ(rows[0].maxcount, 1002u);
    CHECK_EQ(rows[0].incrtime, 1003u);
    CHECK_EQ(rows[0].extendedCost, 1004u);
    CHECK_EQ(rows[0].conditionId, uint16(10005));
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `npc_spellclick_spells` row lands in its member, and the rows come back in
// order.
TEST(NpcDataRepository_SpellClickSpells)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "1002", "1", "1004", "105", "10006"},
        FakeRow{"2000", "2001", "2002", "1", "2004", "205", "20006"},
    });
    std::vector<SpellClickSpellRow> rows = NpcDataRepository::QuerySpellClickSpells();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].npcEntry, 1000u);
    CHECK_EQ(rows[0].spellId, 1001u);
    CHECK_EQ(rows[0].questStart, 1002u);
    CHECK_EQ(rows[0].questStartActive, true);
    CHECK_EQ(rows[0].questEnd, 1004u);
    CHECK_EQ(rows[0].castFlags, uint8(105));
    CHECK_EQ(rows[0].conditionId, uint16(10006));
    CHECK_EQ(rows[1].npcEntry, 2000u);
}

// Each column of a `points_of_interest` row lands in its member, and the rows come back in order.
TEST(NpcDataRepository_PointsOfInterest)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001.5", "1002.5", "1003", "1004", "1005", "s1.6"},
        FakeRow{"2000", "2001.5", "2002.5", "2003", "2004", "2005", "s2.6"},
    });
    std::vector<PointOfInterestRow> rows = NpcDataRepository::QueryPointsOfInterest();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].x, 1001.5f);
    CHECK_EQ(rows[0].y, 1002.5f);
    CHECK_EQ(rows[0].icon, 1003u);
    CHECK_EQ(rows[0].flags, 1004u);
    CHECK_EQ(rows[0].data, 1005u);
    CHECK_STR(rows[0].iconName, "s1.6");
    CHECK_EQ(rows[1].entry, 2000u);
}
