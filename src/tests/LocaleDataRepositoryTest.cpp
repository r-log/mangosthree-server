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

/// LocaleDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/locales/LocaleDataRepository.h"

#include <string>
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

// Each column of a `locales_creature` row lands in its member, and the rows come back in order.
TEST(LocaleDataRepository_CreatureLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22"},
    });
    std::vector<CreatureLocaleRow> rows = LocaleDataRepository::QueryCreatureLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].name[0], "s1.1");
    CHECK_STR(rows[0].subname[0], "s1.2");
    CHECK_STR(rows[0].name[1], "s1.3");
    CHECK_STR(rows[0].subname[1], "s1.4");
    CHECK_STR(rows[0].name[2], "s1.5");
    CHECK_STR(rows[0].subname[2], "s1.6");
    CHECK_STR(rows[0].name[3], "s1.7");
    CHECK_STR(rows[0].subname[3], "s1.8");
    CHECK_STR(rows[0].name[4], "s1.9");
    CHECK_STR(rows[0].subname[4], "s1.10");
    CHECK_STR(rows[0].name[5], "s1.11");
    CHECK_STR(rows[0].subname[5], "s1.12");
    CHECK_STR(rows[0].name[6], "s1.13");
    CHECK_STR(rows[0].subname[6], "s1.14");
    CHECK_STR(rows[0].name[7], "s1.15");
    CHECK_STR(rows[0].subname[7], "s1.16");
    CHECK_STR(rows[0].name[8], "s1.17");
    CHECK_STR(rows[0].subname[8], "s1.18");
    CHECK_STR(rows[0].name[9], "s1.19");
    CHECK_STR(rows[0].subname[9], "s1.20");
    CHECK_STR(rows[0].name[10], "s1.21");
    CHECK_STR(rows[0].subname[10], "s1.22");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_gossip_menu_option` row lands in its member, and the rows come back in
// order.
TEST(LocaleDataRepository_GossipMenuOptionLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"10000", "10001", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22", "s1.23"},
        FakeRow{"20000", "20001", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22", "s2.23"},
    });
    std::vector<GossipMenuOptionLocaleRow> rows = LocaleDataRepository::QueryGossipMenuOptionLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].menuId, uint16(10000));
    CHECK_EQ(rows[0].id, uint16(10001));
    CHECK_STR(rows[0].optionText[0], "s1.2");
    CHECK_STR(rows[0].boxText[0], "s1.3");
    CHECK_STR(rows[0].optionText[1], "s1.4");
    CHECK_STR(rows[0].boxText[1], "s1.5");
    CHECK_STR(rows[0].optionText[2], "s1.6");
    CHECK_STR(rows[0].boxText[2], "s1.7");
    CHECK_STR(rows[0].optionText[3], "s1.8");
    CHECK_STR(rows[0].boxText[3], "s1.9");
    CHECK_STR(rows[0].optionText[4], "s1.10");
    CHECK_STR(rows[0].boxText[4], "s1.11");
    CHECK_STR(rows[0].optionText[5], "s1.12");
    CHECK_STR(rows[0].boxText[5], "s1.13");
    CHECK_STR(rows[0].optionText[6], "s1.14");
    CHECK_STR(rows[0].boxText[6], "s1.15");
    CHECK_STR(rows[0].optionText[7], "s1.16");
    CHECK_STR(rows[0].boxText[7], "s1.17");
    CHECK_STR(rows[0].optionText[8], "s1.18");
    CHECK_STR(rows[0].boxText[8], "s1.19");
    CHECK_STR(rows[0].optionText[9], "s1.20");
    CHECK_STR(rows[0].boxText[9], "s1.21");
    CHECK_STR(rows[0].optionText[10], "s1.22");
    CHECK_STR(rows[0].boxText[10], "s1.23");
    CHECK_EQ(rows[1].menuId, uint16(20000));
}

// Each column of a `locales_points_of_interest` row lands in its member, and the rows come back in
// order.
TEST(LocaleDataRepository_PointOfInterestLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11"},
    });
    std::vector<PointOfInterestLocaleRow> rows = LocaleDataRepository::QueryPointOfInterestLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].iconName[0], "s1.1");
    CHECK_STR(rows[0].iconName[1], "s1.2");
    CHECK_STR(rows[0].iconName[2], "s1.3");
    CHECK_STR(rows[0].iconName[3], "s1.4");
    CHECK_STR(rows[0].iconName[4], "s1.5");
    CHECK_STR(rows[0].iconName[5], "s1.6");
    CHECK_STR(rows[0].iconName[6], "s1.7");
    CHECK_STR(rows[0].iconName[7], "s1.8");
    CHECK_STR(rows[0].iconName[8], "s1.9");
    CHECK_STR(rows[0].iconName[9], "s1.10");
    CHECK_STR(rows[0].iconName[10], "s1.11");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_page_text` row lands in its member, and the rows come back in order.
TEST(LocaleDataRepository_PageTextLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11"},
    });
    std::vector<PageTextLocaleRow> rows = LocaleDataRepository::QueryPageTextLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].text[0], "s1.1");
    CHECK_STR(rows[0].text[1], "s1.2");
    CHECK_STR(rows[0].text[2], "s1.3");
    CHECK_STR(rows[0].text[3], "s1.4");
    CHECK_STR(rows[0].text[4], "s1.5");
    CHECK_STR(rows[0].text[5], "s1.6");
    CHECK_STR(rows[0].text[6], "s1.7");
    CHECK_STR(rows[0].text[7], "s1.8");
    CHECK_STR(rows[0].text[8], "s1.9");
    CHECK_STR(rows[0].text[9], "s1.10");
    CHECK_STR(rows[0].text[10], "s1.11");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_npc_text` row lands in its member, and the rows come back in order.
TEST(LocaleDataRepository_NpcTextLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22", "s1.23", "s1.24", "s1.25", "s1.26", "s1.27",
            "s1.28", "s1.29", "s1.30", "s1.31", "s1.32", "s1.33", "s1.34", "s1.35", "s1.36",
            "s1.37", "s1.38", "s1.39", "s1.40", "s1.41", "s1.42", "s1.43", "s1.44", "s1.45",
            "s1.46", "s1.47", "s1.48", "s1.49", "s1.50", "s1.51", "s1.52", "s1.53", "s1.54",
            "s1.55", "s1.56", "s1.57", "s1.58", "s1.59", "s1.60", "s1.61", "s1.62", "s1.63",
            "s1.64", "s1.65", "s1.66", "s1.67", "s1.68", "s1.69", "s1.70", "s1.71", "s1.72",
            "s1.73", "s1.74", "s1.75", "s1.76", "s1.77", "s1.78", "s1.79", "s1.80", "s1.81",
            "s1.82", "s1.83", "s1.84", "s1.85", "s1.86", "s1.87", "s1.88", "s1.89", "s1.90",
            "s1.91", "s1.92", "s1.93", "s1.94", "s1.95", "s1.96", "s1.97", "s1.98", "s1.99",
            "s1.100", "s1.101", "s1.102", "s1.103", "s1.104", "s1.105", "s1.106", "s1.107",
            "s1.108", "s1.109", "s1.110", "s1.111", "s1.112", "s1.113", "s1.114", "s1.115",
            "s1.116", "s1.117", "s1.118", "s1.119", "s1.120", "s1.121", "s1.122", "s1.123",
            "s1.124", "s1.125", "s1.126", "s1.127", "s1.128", "s1.129", "s1.130", "s1.131",
            "s1.132", "s1.133", "s1.134", "s1.135", "s1.136", "s1.137", "s1.138", "s1.139",
            "s1.140", "s1.141", "s1.142", "s1.143", "s1.144", "s1.145", "s1.146", "s1.147",
            "s1.148", "s1.149", "s1.150", "s1.151", "s1.152", "s1.153", "s1.154", "s1.155",
            "s1.156", "s1.157", "s1.158", "s1.159", "s1.160", "s1.161", "s1.162", "s1.163",
            "s1.164", "s1.165", "s1.166", "s1.167", "s1.168", "s1.169", "s1.170", "s1.171",
            "s1.172", "s1.173", "s1.174", "s1.175", "s1.176"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22", "s2.23", "s2.24", "s2.25", "s2.26", "s2.27",
            "s2.28", "s2.29", "s2.30", "s2.31", "s2.32", "s2.33", "s2.34", "s2.35", "s2.36",
            "s2.37", "s2.38", "s2.39", "s2.40", "s2.41", "s2.42", "s2.43", "s2.44", "s2.45",
            "s2.46", "s2.47", "s2.48", "s2.49", "s2.50", "s2.51", "s2.52", "s2.53", "s2.54",
            "s2.55", "s2.56", "s2.57", "s2.58", "s2.59", "s2.60", "s2.61", "s2.62", "s2.63",
            "s2.64", "s2.65", "s2.66", "s2.67", "s2.68", "s2.69", "s2.70", "s2.71", "s2.72",
            "s2.73", "s2.74", "s2.75", "s2.76", "s2.77", "s2.78", "s2.79", "s2.80", "s2.81",
            "s2.82", "s2.83", "s2.84", "s2.85", "s2.86", "s2.87", "s2.88", "s2.89", "s2.90",
            "s2.91", "s2.92", "s2.93", "s2.94", "s2.95", "s2.96", "s2.97", "s2.98", "s2.99",
            "s2.100", "s2.101", "s2.102", "s2.103", "s2.104", "s2.105", "s2.106", "s2.107",
            "s2.108", "s2.109", "s2.110", "s2.111", "s2.112", "s2.113", "s2.114", "s2.115",
            "s2.116", "s2.117", "s2.118", "s2.119", "s2.120", "s2.121", "s2.122", "s2.123",
            "s2.124", "s2.125", "s2.126", "s2.127", "s2.128", "s2.129", "s2.130", "s2.131",
            "s2.132", "s2.133", "s2.134", "s2.135", "s2.136", "s2.137", "s2.138", "s2.139",
            "s2.140", "s2.141", "s2.142", "s2.143", "s2.144", "s2.145", "s2.146", "s2.147",
            "s2.148", "s2.149", "s2.150", "s2.151", "s2.152", "s2.153", "s2.154", "s2.155",
            "s2.156", "s2.157", "s2.158", "s2.159", "s2.160", "s2.161", "s2.162", "s2.163",
            "s2.164", "s2.165", "s2.166", "s2.167", "s2.168", "s2.169", "s2.170", "s2.171",
            "s2.172", "s2.173", "s2.174", "s2.175", "s2.176"},
    });
    std::vector<NpcTextLocaleRow> rows = LocaleDataRepository::QueryNpcTextLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].text0[0][0], "s1.1");
    CHECK_STR(rows[0].text1[0][0], "s1.2");
    CHECK_STR(rows[0].text0[0][1], "s1.3");
    CHECK_STR(rows[0].text1[0][1], "s1.4");
    CHECK_STR(rows[0].text0[0][2], "s1.5");
    CHECK_STR(rows[0].text1[0][2], "s1.6");
    CHECK_STR(rows[0].text0[0][3], "s1.7");
    CHECK_STR(rows[0].text1[0][3], "s1.8");
    CHECK_STR(rows[0].text0[0][4], "s1.9");
    CHECK_STR(rows[0].text1[0][4], "s1.10");
    CHECK_STR(rows[0].text0[0][5], "s1.11");
    CHECK_STR(rows[0].text1[0][5], "s1.12");
    CHECK_STR(rows[0].text0[0][6], "s1.13");
    CHECK_STR(rows[0].text1[0][6], "s1.14");
    CHECK_STR(rows[0].text0[0][7], "s1.15");
    CHECK_STR(rows[0].text1[0][7], "s1.16");
    CHECK_STR(rows[0].text0[1][0], "s1.17");
    CHECK_STR(rows[0].text1[1][0], "s1.18");
    CHECK_STR(rows[0].text0[1][1], "s1.19");
    CHECK_STR(rows[0].text1[1][1], "s1.20");
    CHECK_STR(rows[0].text0[1][2], "s1.21");
    CHECK_STR(rows[0].text1[1][2], "s1.22");
    CHECK_STR(rows[0].text0[1][3], "s1.23");
    CHECK_STR(rows[0].text1[1][3], "s1.24");
    CHECK_STR(rows[0].text0[1][4], "s1.25");
    CHECK_STR(rows[0].text1[1][4], "s1.26");
    CHECK_STR(rows[0].text0[1][5], "s1.27");
    CHECK_STR(rows[0].text1[1][5], "s1.28");
    CHECK_STR(rows[0].text0[1][6], "s1.29");
    CHECK_STR(rows[0].text1[1][6], "s1.30");
    CHECK_STR(rows[0].text0[1][7], "s1.31");
    CHECK_STR(rows[0].text1[1][7], "s1.32");
    CHECK_STR(rows[0].text0[2][0], "s1.33");
    CHECK_STR(rows[0].text1[2][0], "s1.34");
    CHECK_STR(rows[0].text0[2][1], "s1.35");
    CHECK_STR(rows[0].text1[2][1], "s1.36");
    CHECK_STR(rows[0].text0[2][2], "s1.37");
    CHECK_STR(rows[0].text1[2][2], "s1.38");
    CHECK_STR(rows[0].text0[2][3], "s1.39");
    CHECK_STR(rows[0].text1[2][3], "s1.40");
    CHECK_STR(rows[0].text0[2][4], "s1.41");
    CHECK_STR(rows[0].text1[2][4], "s1.42");
    CHECK_STR(rows[0].text0[2][5], "s1.43");
    CHECK_STR(rows[0].text1[2][5], "s1.44");
    CHECK_STR(rows[0].text0[2][6], "s1.45");
    CHECK_STR(rows[0].text1[2][6], "s1.46");
    CHECK_STR(rows[0].text0[2][7], "s1.47");
    CHECK_STR(rows[0].text1[2][7], "s1.48");
    CHECK_STR(rows[0].text0[3][0], "s1.49");
    CHECK_STR(rows[0].text1[3][0], "s1.50");
    CHECK_STR(rows[0].text0[3][1], "s1.51");
    CHECK_STR(rows[0].text1[3][1], "s1.52");
    CHECK_STR(rows[0].text0[3][2], "s1.53");
    CHECK_STR(rows[0].text1[3][2], "s1.54");
    CHECK_STR(rows[0].text0[3][3], "s1.55");
    CHECK_STR(rows[0].text1[3][3], "s1.56");
    CHECK_STR(rows[0].text0[3][4], "s1.57");
    CHECK_STR(rows[0].text1[3][4], "s1.58");
    CHECK_STR(rows[0].text0[3][5], "s1.59");
    CHECK_STR(rows[0].text1[3][5], "s1.60");
    CHECK_STR(rows[0].text0[3][6], "s1.61");
    CHECK_STR(rows[0].text1[3][6], "s1.62");
    CHECK_STR(rows[0].text0[3][7], "s1.63");
    CHECK_STR(rows[0].text1[3][7], "s1.64");
    CHECK_STR(rows[0].text0[4][0], "s1.65");
    CHECK_STR(rows[0].text1[4][0], "s1.66");
    CHECK_STR(rows[0].text0[4][1], "s1.67");
    CHECK_STR(rows[0].text1[4][1], "s1.68");
    CHECK_STR(rows[0].text0[4][2], "s1.69");
    CHECK_STR(rows[0].text1[4][2], "s1.70");
    CHECK_STR(rows[0].text0[4][3], "s1.71");
    CHECK_STR(rows[0].text1[4][3], "s1.72");
    CHECK_STR(rows[0].text0[4][4], "s1.73");
    CHECK_STR(rows[0].text1[4][4], "s1.74");
    CHECK_STR(rows[0].text0[4][5], "s1.75");
    CHECK_STR(rows[0].text1[4][5], "s1.76");
    CHECK_STR(rows[0].text0[4][6], "s1.77");
    CHECK_STR(rows[0].text1[4][6], "s1.78");
    CHECK_STR(rows[0].text0[4][7], "s1.79");
    CHECK_STR(rows[0].text1[4][7], "s1.80");
    CHECK_STR(rows[0].text0[5][0], "s1.81");
    CHECK_STR(rows[0].text1[5][0], "s1.82");
    CHECK_STR(rows[0].text0[5][1], "s1.83");
    CHECK_STR(rows[0].text1[5][1], "s1.84");
    CHECK_STR(rows[0].text0[5][2], "s1.85");
    CHECK_STR(rows[0].text1[5][2], "s1.86");
    CHECK_STR(rows[0].text0[5][3], "s1.87");
    CHECK_STR(rows[0].text1[5][3], "s1.88");
    CHECK_STR(rows[0].text0[5][4], "s1.89");
    CHECK_STR(rows[0].text1[5][4], "s1.90");
    CHECK_STR(rows[0].text0[5][5], "s1.91");
    CHECK_STR(rows[0].text1[5][5], "s1.92");
    CHECK_STR(rows[0].text0[5][6], "s1.93");
    CHECK_STR(rows[0].text1[5][6], "s1.94");
    CHECK_STR(rows[0].text0[5][7], "s1.95");
    CHECK_STR(rows[0].text1[5][7], "s1.96");
    CHECK_STR(rows[0].text0[6][0], "s1.97");
    CHECK_STR(rows[0].text1[6][0], "s1.98");
    CHECK_STR(rows[0].text0[6][1], "s1.99");
    CHECK_STR(rows[0].text1[6][1], "s1.100");
    CHECK_STR(rows[0].text0[6][2], "s1.101");
    CHECK_STR(rows[0].text1[6][2], "s1.102");
    CHECK_STR(rows[0].text0[6][3], "s1.103");
    CHECK_STR(rows[0].text1[6][3], "s1.104");
    CHECK_STR(rows[0].text0[6][4], "s1.105");
    CHECK_STR(rows[0].text1[6][4], "s1.106");
    CHECK_STR(rows[0].text0[6][5], "s1.107");
    CHECK_STR(rows[0].text1[6][5], "s1.108");
    CHECK_STR(rows[0].text0[6][6], "s1.109");
    CHECK_STR(rows[0].text1[6][6], "s1.110");
    CHECK_STR(rows[0].text0[6][7], "s1.111");
    CHECK_STR(rows[0].text1[6][7], "s1.112");
    CHECK_STR(rows[0].text0[7][0], "s1.113");
    CHECK_STR(rows[0].text1[7][0], "s1.114");
    CHECK_STR(rows[0].text0[7][1], "s1.115");
    CHECK_STR(rows[0].text1[7][1], "s1.116");
    CHECK_STR(rows[0].text0[7][2], "s1.117");
    CHECK_STR(rows[0].text1[7][2], "s1.118");
    CHECK_STR(rows[0].text0[7][3], "s1.119");
    CHECK_STR(rows[0].text1[7][3], "s1.120");
    CHECK_STR(rows[0].text0[7][4], "s1.121");
    CHECK_STR(rows[0].text1[7][4], "s1.122");
    CHECK_STR(rows[0].text0[7][5], "s1.123");
    CHECK_STR(rows[0].text1[7][5], "s1.124");
    CHECK_STR(rows[0].text0[7][6], "s1.125");
    CHECK_STR(rows[0].text1[7][6], "s1.126");
    CHECK_STR(rows[0].text0[7][7], "s1.127");
    CHECK_STR(rows[0].text1[7][7], "s1.128");
    CHECK_STR(rows[0].text0[8][0], "s1.129");
    CHECK_STR(rows[0].text1[8][0], "s1.130");
    CHECK_STR(rows[0].text0[8][1], "s1.131");
    CHECK_STR(rows[0].text1[8][1], "s1.132");
    CHECK_STR(rows[0].text0[8][2], "s1.133");
    CHECK_STR(rows[0].text1[8][2], "s1.134");
    CHECK_STR(rows[0].text0[8][3], "s1.135");
    CHECK_STR(rows[0].text1[8][3], "s1.136");
    CHECK_STR(rows[0].text0[8][4], "s1.137");
    CHECK_STR(rows[0].text1[8][4], "s1.138");
    CHECK_STR(rows[0].text0[8][5], "s1.139");
    CHECK_STR(rows[0].text1[8][5], "s1.140");
    CHECK_STR(rows[0].text0[8][6], "s1.141");
    CHECK_STR(rows[0].text1[8][6], "s1.142");
    CHECK_STR(rows[0].text0[8][7], "s1.143");
    CHECK_STR(rows[0].text1[8][7], "s1.144");
    CHECK_STR(rows[0].text0[9][0], "s1.145");
    CHECK_STR(rows[0].text1[9][0], "s1.146");
    CHECK_STR(rows[0].text0[9][1], "s1.147");
    CHECK_STR(rows[0].text1[9][1], "s1.148");
    CHECK_STR(rows[0].text0[9][2], "s1.149");
    CHECK_STR(rows[0].text1[9][2], "s1.150");
    CHECK_STR(rows[0].text0[9][3], "s1.151");
    CHECK_STR(rows[0].text1[9][3], "s1.152");
    CHECK_STR(rows[0].text0[9][4], "s1.153");
    CHECK_STR(rows[0].text1[9][4], "s1.154");
    CHECK_STR(rows[0].text0[9][5], "s1.155");
    CHECK_STR(rows[0].text1[9][5], "s1.156");
    CHECK_STR(rows[0].text0[9][6], "s1.157");
    CHECK_STR(rows[0].text1[9][6], "s1.158");
    CHECK_STR(rows[0].text0[9][7], "s1.159");
    CHECK_STR(rows[0].text1[9][7], "s1.160");
    CHECK_STR(rows[0].text0[10][0], "s1.161");
    CHECK_STR(rows[0].text1[10][0], "s1.162");
    CHECK_STR(rows[0].text0[10][1], "s1.163");
    CHECK_STR(rows[0].text1[10][1], "s1.164");
    CHECK_STR(rows[0].text0[10][2], "s1.165");
    CHECK_STR(rows[0].text1[10][2], "s1.166");
    CHECK_STR(rows[0].text0[10][3], "s1.167");
    CHECK_STR(rows[0].text1[10][3], "s1.168");
    CHECK_STR(rows[0].text0[10][4], "s1.169");
    CHECK_STR(rows[0].text1[10][4], "s1.170");
    CHECK_STR(rows[0].text0[10][5], "s1.171");
    CHECK_STR(rows[0].text1[10][5], "s1.172");
    CHECK_STR(rows[0].text0[10][6], "s1.173");
    CHECK_STR(rows[0].text1[10][6], "s1.174");
    CHECK_STR(rows[0].text0[10][7], "s1.175");
    CHECK_STR(rows[0].text1[10][7], "s1.176");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_gameobject` row lands in its member, and the rows come back in order.
TEST(LocaleDataRepository_GameObjectLocales)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "s1.13", "s1.14", "s1.15", "s1.16", "s1.17", "s1.18",
            "s1.19", "s1.20", "s1.21", "s1.22"},
        FakeRow{"2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "s2.13", "s2.14", "s2.15", "s2.16", "s2.17", "s2.18",
            "s2.19", "s2.20", "s2.21", "s2.22"},
    });
    std::vector<GameObjectLocaleRow> rows = LocaleDataRepository::QueryGameObjectLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].name[0], "s1.1");
    CHECK_STR(rows[0].name[1], "s1.2");
    CHECK_STR(rows[0].name[2], "s1.3");
    CHECK_STR(rows[0].name[3], "s1.4");
    CHECK_STR(rows[0].name[4], "s1.5");
    CHECK_STR(rows[0].name[5], "s1.6");
    CHECK_STR(rows[0].name[6], "s1.7");
    CHECK_STR(rows[0].name[7], "s1.8");
    CHECK_STR(rows[0].name[8], "s1.9");
    CHECK_STR(rows[0].name[9], "s1.10");
    CHECK_STR(rows[0].name[10], "s1.11");
    CHECK_STR(rows[0].castBarCaption[0], "s1.12");
    CHECK_STR(rows[0].castBarCaption[1], "s1.13");
    CHECK_STR(rows[0].castBarCaption[2], "s1.14");
    CHECK_STR(rows[0].castBarCaption[3], "s1.15");
    CHECK_STR(rows[0].castBarCaption[4], "s1.16");
    CHECK_STR(rows[0].castBarCaption[5], "s1.17");
    CHECK_STR(rows[0].castBarCaption[6], "s1.18");
    CHECK_STR(rows[0].castBarCaption[7], "s1.19");
    CHECK_STR(rows[0].castBarCaption[8], "s1.20");
    CHECK_STR(rows[0].castBarCaption[9], "s1.21");
    CHECK_STR(rows[0].castBarCaption[10], "s1.22");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a string row lands in its member; the extra columns are read only when they are
// asked for, and the query names the table it was handed.
TEST(LocaleDataRepository_Strings)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"-1000", "s1.1", "s1.2", "s1.3", "s1.4", "s1.5", "s1.6", "s1.7", "s1.8", "s1.9",
            "s1.10", "s1.11", "s1.12", "1013", "1014", "1015", "1016"},
        FakeRow{"-2000", "s2.1", "s2.2", "s2.3", "s2.4", "s2.5", "s2.6", "s2.7", "s2.8", "s2.9",
            "s2.10", "s2.11", "s2.12", "2013", "2014", "2015", "2016"},
    });
    std::vector<StringRow> plain = LocaleDataRepository::QueryStrings(WorldDatabase, "mangos_string", false);
    std::vector<StringRow> extra = LocaleDataRepository::QueryStrings(WorldDatabase, "script_texts", true);
    REQUIRE(plain.size() == 2);
    CHECK_EQ(plain[0].entry, -1000);
    CHECK_STR(plain[0].contentDefault, "s1.1");
    CHECK_STR(plain[0].content[0], "s1.2");
    CHECK_STR(plain[0].content[10], "s1.12");
    CHECK_EQ(plain[0].sound, 0u);
    CHECK_EQ(plain[0].emote, 0u);
    CHECK_EQ(plain[1].entry, -2000);
    REQUIRE(extra.size() == 2);
    CHECK_EQ(extra[0].sound, 1013u);
    CHECK_EQ(extra[0].type, 1014u);
    CHECK_EQ(extra[0].language, 1015u);
    CHECK_EQ(extra[0].emote, 1016u);
    REQUIRE(db.query.executed.size() == 2);
    CHECK(db.query.executed[0].find(" FROM mangos_string") != std::string::npos);
    CHECK(db.query.executed[1].find(",sound,type,language,emote FROM script_texts") != std::string::npos);
}
