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

// Each column of an `npc_text` row lands in its member, and the rows come back in order.
TEST(NpcDataRepository_NpcTexts)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "1003", "1004.5", "1005", "1006", "1007", "1008", "1009",
            "1010", "s1.11", "s1.12", "1013", "1014.5", "1015", "1016", "1017", "1018", "1019",
            "1020", "s1.21", "s1.22", "1023", "1024.5", "1025", "1026", "1027", "1028", "1029",
            "1030", "s1.31", "s1.32", "1033", "1034.5", "1035", "1036", "1037", "1038", "1039",
            "1040", "s1.41", "s1.42", "1043", "1044.5", "1045", "1046", "1047", "1048", "1049",
            "1050", "s1.51", "s1.52", "1053", "1054.5", "1055", "1056", "1057", "1058", "1059",
            "1060", "s1.61", "s1.62", "1063", "1064.5", "1065", "1066", "1067", "1068", "1069",
            "1070", "s1.71", "s1.72", "1073", "1074.5", "1075", "1076", "1077", "1078", "1079",
            "1080"},
        FakeRow{"2000", "s2.1", "s2.2", "2003", "2004.5", "2005", "2006", "2007", "2008", "2009",
            "2010", "s2.11", "s2.12", "2013", "2014.5", "2015", "2016", "2017", "2018", "2019",
            "2020", "s2.21", "s2.22", "2023", "2024.5", "2025", "2026", "2027", "2028", "2029",
            "2030", "s2.31", "s2.32", "2033", "2034.5", "2035", "2036", "2037", "2038", "2039",
            "2040", "s2.41", "s2.42", "2043", "2044.5", "2045", "2046", "2047", "2048", "2049",
            "2050", "s2.51", "s2.52", "2053", "2054.5", "2055", "2056", "2057", "2058", "2059",
            "2060", "s2.61", "s2.62", "2063", "2064.5", "2065", "2066", "2067", "2068", "2069",
            "2070", "s2.71", "s2.72", "2073", "2074.5", "2075", "2076", "2077", "2078", "2079",
            "2080"},
    });
    std::vector<NpcTextRow> rows = NpcDataRepository::QueryNpcTexts();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].id, 1000u);
    CHECK_STR(rows[0].options[0].text0, "s1.1");
    CHECK_STR(rows[0].options[0].text1, "s1.2");
    CHECK_EQ(rows[0].options[0].language, 1003u);
    CHECK_EQ(rows[0].options[0].probability, 1004.5f);
    CHECK_EQ(rows[0].options[0].emoteDelay[0], 1005u);
    CHECK_EQ(rows[0].options[0].emote[0], 1006u);
    CHECK_EQ(rows[0].options[0].emoteDelay[1], 1007u);
    CHECK_EQ(rows[0].options[0].emote[1], 1008u);
    CHECK_EQ(rows[0].options[0].emoteDelay[2], 1009u);
    CHECK_EQ(rows[0].options[0].emote[2], 1010u);
    CHECK_STR(rows[0].options[1].text0, "s1.11");
    CHECK_STR(rows[0].options[1].text1, "s1.12");
    CHECK_EQ(rows[0].options[1].language, 1013u);
    CHECK_EQ(rows[0].options[1].probability, 1014.5f);
    CHECK_EQ(rows[0].options[1].emoteDelay[0], 1015u);
    CHECK_EQ(rows[0].options[1].emote[0], 1016u);
    CHECK_EQ(rows[0].options[1].emoteDelay[1], 1017u);
    CHECK_EQ(rows[0].options[1].emote[1], 1018u);
    CHECK_EQ(rows[0].options[1].emoteDelay[2], 1019u);
    CHECK_EQ(rows[0].options[1].emote[2], 1020u);
    CHECK_STR(rows[0].options[2].text0, "s1.21");
    CHECK_STR(rows[0].options[2].text1, "s1.22");
    CHECK_EQ(rows[0].options[2].language, 1023u);
    CHECK_EQ(rows[0].options[2].probability, 1024.5f);
    CHECK_EQ(rows[0].options[2].emoteDelay[0], 1025u);
    CHECK_EQ(rows[0].options[2].emote[0], 1026u);
    CHECK_EQ(rows[0].options[2].emoteDelay[1], 1027u);
    CHECK_EQ(rows[0].options[2].emote[1], 1028u);
    CHECK_EQ(rows[0].options[2].emoteDelay[2], 1029u);
    CHECK_EQ(rows[0].options[2].emote[2], 1030u);
    CHECK_STR(rows[0].options[3].text0, "s1.31");
    CHECK_STR(rows[0].options[3].text1, "s1.32");
    CHECK_EQ(rows[0].options[3].language, 1033u);
    CHECK_EQ(rows[0].options[3].probability, 1034.5f);
    CHECK_EQ(rows[0].options[3].emoteDelay[0], 1035u);
    CHECK_EQ(rows[0].options[3].emote[0], 1036u);
    CHECK_EQ(rows[0].options[3].emoteDelay[1], 1037u);
    CHECK_EQ(rows[0].options[3].emote[1], 1038u);
    CHECK_EQ(rows[0].options[3].emoteDelay[2], 1039u);
    CHECK_EQ(rows[0].options[3].emote[2], 1040u);
    CHECK_STR(rows[0].options[4].text0, "s1.41");
    CHECK_STR(rows[0].options[4].text1, "s1.42");
    CHECK_EQ(rows[0].options[4].language, 1043u);
    CHECK_EQ(rows[0].options[4].probability, 1044.5f);
    CHECK_EQ(rows[0].options[4].emoteDelay[0], 1045u);
    CHECK_EQ(rows[0].options[4].emote[0], 1046u);
    CHECK_EQ(rows[0].options[4].emoteDelay[1], 1047u);
    CHECK_EQ(rows[0].options[4].emote[1], 1048u);
    CHECK_EQ(rows[0].options[4].emoteDelay[2], 1049u);
    CHECK_EQ(rows[0].options[4].emote[2], 1050u);
    CHECK_STR(rows[0].options[5].text0, "s1.51");
    CHECK_STR(rows[0].options[5].text1, "s1.52");
    CHECK_EQ(rows[0].options[5].language, 1053u);
    CHECK_EQ(rows[0].options[5].probability, 1054.5f);
    CHECK_EQ(rows[0].options[5].emoteDelay[0], 1055u);
    CHECK_EQ(rows[0].options[5].emote[0], 1056u);
    CHECK_EQ(rows[0].options[5].emoteDelay[1], 1057u);
    CHECK_EQ(rows[0].options[5].emote[1], 1058u);
    CHECK_EQ(rows[0].options[5].emoteDelay[2], 1059u);
    CHECK_EQ(rows[0].options[5].emote[2], 1060u);
    CHECK_STR(rows[0].options[6].text0, "s1.61");
    CHECK_STR(rows[0].options[6].text1, "s1.62");
    CHECK_EQ(rows[0].options[6].language, 1063u);
    CHECK_EQ(rows[0].options[6].probability, 1064.5f);
    CHECK_EQ(rows[0].options[6].emoteDelay[0], 1065u);
    CHECK_EQ(rows[0].options[6].emote[0], 1066u);
    CHECK_EQ(rows[0].options[6].emoteDelay[1], 1067u);
    CHECK_EQ(rows[0].options[6].emote[1], 1068u);
    CHECK_EQ(rows[0].options[6].emoteDelay[2], 1069u);
    CHECK_EQ(rows[0].options[6].emote[2], 1070u);
    CHECK_STR(rows[0].options[7].text0, "s1.71");
    CHECK_STR(rows[0].options[7].text1, "s1.72");
    CHECK_EQ(rows[0].options[7].language, 1073u);
    CHECK_EQ(rows[0].options[7].probability, 1074.5f);
    CHECK_EQ(rows[0].options[7].emoteDelay[0], 1075u);
    CHECK_EQ(rows[0].options[7].emote[0], 1076u);
    CHECK_EQ(rows[0].options[7].emoteDelay[1], 1077u);
    CHECK_EQ(rows[0].options[7].emote[1], 1078u);
    CHECK_EQ(rows[0].options[7].emoteDelay[2], 1079u);
    CHECK_EQ(rows[0].options[7].emote[2], 1080u);
    CHECK_EQ(rows[1].id, 2000u);
}
