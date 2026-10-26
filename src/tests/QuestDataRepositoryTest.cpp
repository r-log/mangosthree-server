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

/// QuestDataRepository's row reads on the global WorldDatabase backed by fakes: every column of a
/// canned row lands in its member, typed as the loader read it.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/quests/QuestDataRepository.h"

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

// Each column of a `quest_relations` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_QuestRelations)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<QuestRelationRow> rows = QuestDataRepository::QueryQuestRelations(3, 2);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].quest, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `quest_relations` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_ActorQuestRelations)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001"},
        FakeRow{"2000", "2001"},
    });
    std::vector<QuestRelationRow> rows = QuestDataRepository::QueryActorQuestRelations(3);
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].quest, 1001u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `quest_template` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_Quests)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "-1002", "1003", "-1004", "1005", "1006", "1007", "1008", "1009",
            "1010", "-1011", "1012", "-1013", "1014", "-1015", "1016", "1017", "10018", "10019",
            "1020", "1021", "1022", "1023", "1024", "-1025", "-1026", "-1027", "1028", "1029",
            "1030", "1031", "1032", "s1.33", "s1.34", "s1.35", "s1.36", "s1.37", "s1.38",
            "s1.39", "s1.40", "s1.41", "s1.42", "s1.43", "s1.44", "s1.45", "s1.46", "s1.47",
            "1048", "1049", "1050", "1051", "1052", "1053", "1054", "1055", "1056", "1057",
            "1058", "1059", "1060", "1061", "1062", "1063", "1064", "1065", "1066", "1067",
            "-1068", "-1069", "-1070", "-1071", "1072", "1073", "1074", "1075", "1076", "1077",
            "1078", "1079", "1080", "1081", "1082", "1083", "1084", "1085", "1086", "1087",
            "1088", "1089", "1090", "1091", "1092", "1093", "1094", "1095", "1096", "1097",
            "1098", "1099", "1100", "1101", "1102", "1103", "1104", "1105", "1106", "1107",
            "1108", "1109", "1110", "1111", "1112", "1113", "1114", "1115", "1116", "1117",
            "1118", "1119", "1120", "1121", "1122", "1123", "-1124", "-1125", "-1126", "-1127",
            "-1128", "-1129", "-1130", "-1131", "-1132", "-1133", "1134", "1135.5", "-1136",
            "1137", "1138", "1139", "1140", "1141", "1142", "1143.5", "1144.5", "1145", "1146",
            "1147", "1148", "1149", "1150", "1151", "1152", "1153", "1154", "1155", "-1156",
            "-1157", "-1158", "-1159", "-1160", "-1161", "-1162", "-1163", "1164", "1165",
            "1166", "1167"},
        FakeRow{"2000", "2001", "-2002", "2003", "-2004", "2005", "2006", "2007", "2008", "2009",
            "2010", "-2011", "2012", "-2013", "2014", "-2015", "2016", "2017", "20018", "20019",
            "2020", "2021", "2022", "2023", "2024", "-2025", "-2026", "-2027", "2028", "2029",
            "2030", "2031", "2032", "s2.33", "s2.34", "s2.35", "s2.36", "s2.37", "s2.38",
            "s2.39", "s2.40", "s2.41", "s2.42", "s2.43", "s2.44", "s2.45", "s2.46", "s2.47",
            "2048", "2049", "2050", "2051", "2052", "2053", "2054", "2055", "2056", "2057",
            "2058", "2059", "2060", "2061", "2062", "2063", "2064", "2065", "2066", "2067",
            "-2068", "-2069", "-2070", "-2071", "2072", "2073", "2074", "2075", "2076", "2077",
            "2078", "2079", "2080", "2081", "2082", "2083", "2084", "2085", "2086", "2087",
            "2088", "2089", "2090", "2091", "2092", "2093", "2094", "2095", "2096", "2097",
            "2098", "2099", "2100", "2101", "2102", "2103", "2104", "2105", "2106", "2107",
            "2108", "2109", "2110", "2111", "2112", "2113", "2114", "2115", "2116", "2117",
            "2118", "2119", "2120", "2121", "2122", "2123", "-2124", "-2125", "-2126", "-2127",
            "-2128", "-2129", "-2130", "-2131", "-2132", "-2133", "2134", "2135.5", "-2136",
            "2137", "2138", "2139", "2140", "2141", "2142", "2143.5", "2144.5", "2145", "2146",
            "2147", "2148", "2149", "2150", "2151", "2152", "2153", "2154", "2155", "-2156",
            "-2157", "-2158", "-2159", "-2160", "-2161", "-2162", "-2163", "2164", "2165",
            "2166", "2167"},
    });
    std::vector<QuestTemplateRow> rows = QuestDataRepository::QueryQuests();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_EQ(rows[0].method, 1001u);
    CHECK_EQ(rows[0].zoneOrSort, -1002);
    CHECK_EQ(rows[0].minLevel, 1003u);
    CHECK_EQ(rows[0].questLevel, -1004);
    CHECK_EQ(rows[0].type, 1005u);
    CHECK_EQ(rows[0].requiredClasses, 1006u);
    CHECK_EQ(rows[0].requiredRaces, 1007u);
    CHECK_EQ(rows[0].requiredSkill, 1008u);
    CHECK_EQ(rows[0].requiredSkillValue, 1009u);
    CHECK_EQ(rows[0].repObjectiveFaction, 1010u);
    CHECK_EQ(rows[0].repObjectiveValue, -1011);
    CHECK_EQ(rows[0].requiredMinRepFaction, 1012u);
    CHECK_EQ(rows[0].requiredMinRepValue, -1013);
    CHECK_EQ(rows[0].requiredMaxRepFaction, 1014u);
    CHECK_EQ(rows[0].requiredMaxRepValue, -1015);
    CHECK_EQ(rows[0].suggestedPlayers, 1016u);
    CHECK_EQ(rows[0].limitTime, 1017u);
    CHECK_EQ(rows[0].questFlags, uint16(10018));
    CHECK_EQ(rows[0].specialFlags, uint16(10019));
    CHECK_EQ(rows[0].charTitleId, 1020u);
    CHECK_EQ(rows[0].playersSlain, 1021u);
    CHECK_EQ(rows[0].bonusTalents, 1022u);
    CHECK_EQ(rows[0].portraitGiver, 1023u);
    CHECK_EQ(rows[0].portraitTurnIn, 1024u);
    CHECK_EQ(rows[0].prevQuestId, -1025);
    CHECK_EQ(rows[0].nextQuestId, -1026);
    CHECK_EQ(rows[0].exclusiveGroup, -1027);
    CHECK_EQ(rows[0].nextQuestInChain, 1028u);
    CHECK_EQ(rows[0].rewXPId, 1029u);
    CHECK_EQ(rows[0].srcItemId, 1030u);
    CHECK_EQ(rows[0].srcItemCount, 1031u);
    CHECK_EQ(rows[0].srcSpell, 1032u);
    CHECK_STR(rows[0].title, "s1.33");
    CHECK_STR(rows[0].details, "s1.34");
    CHECK_STR(rows[0].objectives, "s1.35");
    CHECK_STR(rows[0].offerRewardText, "s1.36");
    CHECK_STR(rows[0].requestItemsText, "s1.37");
    CHECK_STR(rows[0].endText, "s1.38");
    CHECK_STR(rows[0].completedText, "s1.39");
    CHECK_STR(rows[0].portraitGiverName, "s1.40");
    CHECK_STR(rows[0].portraitGiverText, "s1.41");
    CHECK_STR(rows[0].portraitTurnInName, "s1.42");
    CHECK_STR(rows[0].portraitTurnInText, "s1.43");
    CHECK_STR(rows[0].objectiveText[0], "s1.44");
    CHECK_STR(rows[0].objectiveText[1], "s1.45");
    CHECK_STR(rows[0].objectiveText[2], "s1.46");
    CHECK_STR(rows[0].objectiveText[3], "s1.47");
    CHECK_EQ(rows[0].reqItemId[0], 1048u);
    CHECK_EQ(rows[0].reqItemId[1], 1049u);
    CHECK_EQ(rows[0].reqItemId[2], 1050u);
    CHECK_EQ(rows[0].reqItemId[3], 1051u);
    CHECK_EQ(rows[0].reqItemId[4], 1052u);
    CHECK_EQ(rows[0].reqItemId[5], 1053u);
    CHECK_EQ(rows[0].reqItemCount[0], 1054u);
    CHECK_EQ(rows[0].reqItemCount[1], 1055u);
    CHECK_EQ(rows[0].reqItemCount[2], 1056u);
    CHECK_EQ(rows[0].reqItemCount[3], 1057u);
    CHECK_EQ(rows[0].reqItemCount[4], 1058u);
    CHECK_EQ(rows[0].reqItemCount[5], 1059u);
    CHECK_EQ(rows[0].reqSourceId[0], 1060u);
    CHECK_EQ(rows[0].reqSourceId[1], 1061u);
    CHECK_EQ(rows[0].reqSourceId[2], 1062u);
    CHECK_EQ(rows[0].reqSourceId[3], 1063u);
    CHECK_EQ(rows[0].reqSourceCount[0], 1064u);
    CHECK_EQ(rows[0].reqSourceCount[1], 1065u);
    CHECK_EQ(rows[0].reqSourceCount[2], 1066u);
    CHECK_EQ(rows[0].reqSourceCount[3], 1067u);
    CHECK_EQ(rows[0].reqCreatureOrGOId[0], -1068);
    CHECK_EQ(rows[0].reqCreatureOrGOId[1], -1069);
    CHECK_EQ(rows[0].reqCreatureOrGOId[2], -1070);
    CHECK_EQ(rows[0].reqCreatureOrGOId[3], -1071);
    CHECK_EQ(rows[0].reqCreatureOrGOCount[0], 1072u);
    CHECK_EQ(rows[0].reqCreatureOrGOCount[1], 1073u);
    CHECK_EQ(rows[0].reqCreatureOrGOCount[2], 1074u);
    CHECK_EQ(rows[0].reqCreatureOrGOCount[3], 1075u);
    CHECK_EQ(rows[0].reqCurrencyId[0], 1076u);
    CHECK_EQ(rows[0].reqCurrencyId[1], 1077u);
    CHECK_EQ(rows[0].reqCurrencyId[2], 1078u);
    CHECK_EQ(rows[0].reqCurrencyId[3], 1079u);
    CHECK_EQ(rows[0].reqCurrencyCount[0], 1080u);
    CHECK_EQ(rows[0].reqCurrencyCount[1], 1081u);
    CHECK_EQ(rows[0].reqCurrencyCount[2], 1082u);
    CHECK_EQ(rows[0].reqCurrencyCount[3], 1083u);
    CHECK_EQ(rows[0].reqSpellCast[0], 1084u);
    CHECK_EQ(rows[0].reqSpellCast[1], 1085u);
    CHECK_EQ(rows[0].reqSpellCast[2], 1086u);
    CHECK_EQ(rows[0].reqSpellCast[3], 1087u);
    CHECK_EQ(rows[0].reqSpellLearned, 1088u);
    CHECK_EQ(rows[0].rewChoiceItemId[0], 1089u);
    CHECK_EQ(rows[0].rewChoiceItemId[1], 1090u);
    CHECK_EQ(rows[0].rewChoiceItemId[2], 1091u);
    CHECK_EQ(rows[0].rewChoiceItemId[3], 1092u);
    CHECK_EQ(rows[0].rewChoiceItemId[4], 1093u);
    CHECK_EQ(rows[0].rewChoiceItemId[5], 1094u);
    CHECK_EQ(rows[0].rewChoiceItemCount[0], 1095u);
    CHECK_EQ(rows[0].rewChoiceItemCount[1], 1096u);
    CHECK_EQ(rows[0].rewChoiceItemCount[2], 1097u);
    CHECK_EQ(rows[0].rewChoiceItemCount[3], 1098u);
    CHECK_EQ(rows[0].rewChoiceItemCount[4], 1099u);
    CHECK_EQ(rows[0].rewChoiceItemCount[5], 1100u);
    CHECK_EQ(rows[0].rewItemId[0], 1101u);
    CHECK_EQ(rows[0].rewItemId[1], 1102u);
    CHECK_EQ(rows[0].rewItemId[2], 1103u);
    CHECK_EQ(rows[0].rewItemId[3], 1104u);
    CHECK_EQ(rows[0].rewItemCount[0], 1105u);
    CHECK_EQ(rows[0].rewItemCount[1], 1106u);
    CHECK_EQ(rows[0].rewItemCount[2], 1107u);
    CHECK_EQ(rows[0].rewItemCount[3], 1108u);
    CHECK_EQ(rows[0].rewCurrencyId[0], 1109u);
    CHECK_EQ(rows[0].rewCurrencyId[1], 1110u);
    CHECK_EQ(rows[0].rewCurrencyId[2], 1111u);
    CHECK_EQ(rows[0].rewCurrencyId[3], 1112u);
    CHECK_EQ(rows[0].rewCurrencyCount[0], 1113u);
    CHECK_EQ(rows[0].rewCurrencyCount[1], 1114u);
    CHECK_EQ(rows[0].rewCurrencyCount[2], 1115u);
    CHECK_EQ(rows[0].rewCurrencyCount[3], 1116u);
    CHECK_EQ(rows[0].rewSkill, 1117u);
    CHECK_EQ(rows[0].rewSkillValue, 1118u);
    CHECK_EQ(rows[0].rewRepFaction[0], 1119u);
    CHECK_EQ(rows[0].rewRepFaction[1], 1120u);
    CHECK_EQ(rows[0].rewRepFaction[2], 1121u);
    CHECK_EQ(rows[0].rewRepFaction[3], 1122u);
    CHECK_EQ(rows[0].rewRepFaction[4], 1123u);
    CHECK_EQ(rows[0].rewRepValueId[0], -1124);
    CHECK_EQ(rows[0].rewRepValueId[1], -1125);
    CHECK_EQ(rows[0].rewRepValueId[2], -1126);
    CHECK_EQ(rows[0].rewRepValueId[3], -1127);
    CHECK_EQ(rows[0].rewRepValueId[4], -1128);
    CHECK_EQ(rows[0].rewRepValue[0], -1129);
    CHECK_EQ(rows[0].rewRepValue[1], -1130);
    CHECK_EQ(rows[0].rewRepValue[2], -1131);
    CHECK_EQ(rows[0].rewRepValue[3], -1132);
    CHECK_EQ(rows[0].rewRepValue[4], -1133);
    CHECK_EQ(rows[0].rewHonorAddition, 1134u);
    CHECK_EQ(rows[0].rewHonorMultiplier, 1135.5f);
    CHECK_EQ(rows[0].rewOrReqMoney, -1136);
    CHECK_EQ(rows[0].rewMoneyMaxLevel, 1137u);
    CHECK_EQ(rows[0].rewSpell, 1138u);
    CHECK_EQ(rows[0].rewSpellCast, 1139u);
    CHECK_EQ(rows[0].rewMailTemplateId, 1140u);
    CHECK_EQ(rows[0].rewMailDelaySecs, 1141u);
    CHECK_EQ(rows[0].pointMapId, 1142u);
    CHECK_EQ(rows[0].pointX, 1143.5f);
    CHECK_EQ(rows[0].pointY, 1144.5f);
    CHECK_EQ(rows[0].pointOpt, 1145u);
    CHECK_EQ(rows[0].detailsEmote[0], 1146u);
    CHECK_EQ(rows[0].detailsEmote[1], 1147u);
    CHECK_EQ(rows[0].detailsEmote[2], 1148u);
    CHECK_EQ(rows[0].detailsEmote[3], 1149u);
    CHECK_EQ(rows[0].detailsEmoteDelay[0], 1150u);
    CHECK_EQ(rows[0].detailsEmoteDelay[1], 1151u);
    CHECK_EQ(rows[0].detailsEmoteDelay[2], 1152u);
    CHECK_EQ(rows[0].detailsEmoteDelay[3], 1153u);
    CHECK_EQ(rows[0].incompleteEmote, 1154u);
    CHECK_EQ(rows[0].completeEmote, 1155u);
    CHECK_EQ(rows[0].offerRewardEmote[0], -1156);
    CHECK_EQ(rows[0].offerRewardEmote[1], -1157);
    CHECK_EQ(rows[0].offerRewardEmote[2], -1158);
    CHECK_EQ(rows[0].offerRewardEmote[3], -1159);
    CHECK_EQ(rows[0].offerRewardEmoteDelay[0], -1160);
    CHECK_EQ(rows[0].offerRewardEmoteDelay[1], -1161);
    CHECK_EQ(rows[0].offerRewardEmoteDelay[2], -1162);
    CHECK_EQ(rows[0].offerRewardEmoteDelay[3], -1163);
    CHECK_EQ(rows[0].soundAccept, 1164u);
    CHECK_EQ(rows[0].soundTurnIn, 1165u);
    CHECK_EQ(rows[0].startScript, 1166u);
    CHECK_EQ(rows[0].completeScript, 1167u);
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `locales_quest` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_QuestLocales)
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
            "s1.164", "s1.165"},
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
            "s2.164", "s2.165"},
    });
    std::vector<QuestLocaleRow> rows = QuestDataRepository::QueryQuestLocales();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].entry, 1000u);
    CHECK_STR(rows[0].title[0], "s1.1");
    CHECK_STR(rows[0].details[0], "s1.2");
    CHECK_STR(rows[0].objectives[0], "s1.3");
    CHECK_STR(rows[0].offerRewardText[0], "s1.4");
    CHECK_STR(rows[0].requestItemsText[0], "s1.5");
    CHECK_STR(rows[0].endText[0], "s1.6");
    CHECK_STR(rows[0].completedText[0], "s1.7");
    CHECK_STR(rows[0].objectiveText[0][0], "s1.8");
    CHECK_STR(rows[0].objectiveText[0][1], "s1.9");
    CHECK_STR(rows[0].objectiveText[0][2], "s1.10");
    CHECK_STR(rows[0].objectiveText[0][3], "s1.11");
    CHECK_STR(rows[0].portraitGiverName[0], "s1.12");
    CHECK_STR(rows[0].portraitGiverText[0], "s1.13");
    CHECK_STR(rows[0].portraitTurnInName[0], "s1.14");
    CHECK_STR(rows[0].portraitTurnInText[0], "s1.15");
    CHECK_STR(rows[0].title[1], "s1.16");
    CHECK_STR(rows[0].details[1], "s1.17");
    CHECK_STR(rows[0].objectives[1], "s1.18");
    CHECK_STR(rows[0].offerRewardText[1], "s1.19");
    CHECK_STR(rows[0].requestItemsText[1], "s1.20");
    CHECK_STR(rows[0].endText[1], "s1.21");
    CHECK_STR(rows[0].completedText[1], "s1.22");
    CHECK_STR(rows[0].objectiveText[1][0], "s1.23");
    CHECK_STR(rows[0].objectiveText[1][1], "s1.24");
    CHECK_STR(rows[0].objectiveText[1][2], "s1.25");
    CHECK_STR(rows[0].objectiveText[1][3], "s1.26");
    CHECK_STR(rows[0].portraitGiverName[1], "s1.27");
    CHECK_STR(rows[0].portraitGiverText[1], "s1.28");
    CHECK_STR(rows[0].portraitTurnInName[1], "s1.29");
    CHECK_STR(rows[0].portraitTurnInText[1], "s1.30");
    CHECK_STR(rows[0].title[2], "s1.31");
    CHECK_STR(rows[0].details[2], "s1.32");
    CHECK_STR(rows[0].objectives[2], "s1.33");
    CHECK_STR(rows[0].offerRewardText[2], "s1.34");
    CHECK_STR(rows[0].requestItemsText[2], "s1.35");
    CHECK_STR(rows[0].endText[2], "s1.36");
    CHECK_STR(rows[0].completedText[2], "s1.37");
    CHECK_STR(rows[0].objectiveText[2][0], "s1.38");
    CHECK_STR(rows[0].objectiveText[2][1], "s1.39");
    CHECK_STR(rows[0].objectiveText[2][2], "s1.40");
    CHECK_STR(rows[0].objectiveText[2][3], "s1.41");
    CHECK_STR(rows[0].portraitGiverName[2], "s1.42");
    CHECK_STR(rows[0].portraitGiverText[2], "s1.43");
    CHECK_STR(rows[0].portraitTurnInName[2], "s1.44");
    CHECK_STR(rows[0].portraitTurnInText[2], "s1.45");
    CHECK_STR(rows[0].title[3], "s1.46");
    CHECK_STR(rows[0].details[3], "s1.47");
    CHECK_STR(rows[0].objectives[3], "s1.48");
    CHECK_STR(rows[0].offerRewardText[3], "s1.49");
    CHECK_STR(rows[0].requestItemsText[3], "s1.50");
    CHECK_STR(rows[0].endText[3], "s1.51");
    CHECK_STR(rows[0].completedText[3], "s1.52");
    CHECK_STR(rows[0].objectiveText[3][0], "s1.53");
    CHECK_STR(rows[0].objectiveText[3][1], "s1.54");
    CHECK_STR(rows[0].objectiveText[3][2], "s1.55");
    CHECK_STR(rows[0].objectiveText[3][3], "s1.56");
    CHECK_STR(rows[0].portraitGiverName[3], "s1.57");
    CHECK_STR(rows[0].portraitGiverText[3], "s1.58");
    CHECK_STR(rows[0].portraitTurnInName[3], "s1.59");
    CHECK_STR(rows[0].portraitTurnInText[3], "s1.60");
    CHECK_STR(rows[0].title[4], "s1.61");
    CHECK_STR(rows[0].details[4], "s1.62");
    CHECK_STR(rows[0].objectives[4], "s1.63");
    CHECK_STR(rows[0].offerRewardText[4], "s1.64");
    CHECK_STR(rows[0].requestItemsText[4], "s1.65");
    CHECK_STR(rows[0].endText[4], "s1.66");
    CHECK_STR(rows[0].completedText[4], "s1.67");
    CHECK_STR(rows[0].objectiveText[4][0], "s1.68");
    CHECK_STR(rows[0].objectiveText[4][1], "s1.69");
    CHECK_STR(rows[0].objectiveText[4][2], "s1.70");
    CHECK_STR(rows[0].objectiveText[4][3], "s1.71");
    CHECK_STR(rows[0].portraitGiverName[4], "s1.72");
    CHECK_STR(rows[0].portraitGiverText[4], "s1.73");
    CHECK_STR(rows[0].portraitTurnInName[4], "s1.74");
    CHECK_STR(rows[0].portraitTurnInText[4], "s1.75");
    CHECK_STR(rows[0].title[5], "s1.76");
    CHECK_STR(rows[0].details[5], "s1.77");
    CHECK_STR(rows[0].objectives[5], "s1.78");
    CHECK_STR(rows[0].offerRewardText[5], "s1.79");
    CHECK_STR(rows[0].requestItemsText[5], "s1.80");
    CHECK_STR(rows[0].endText[5], "s1.81");
    CHECK_STR(rows[0].completedText[5], "s1.82");
    CHECK_STR(rows[0].objectiveText[5][0], "s1.83");
    CHECK_STR(rows[0].objectiveText[5][1], "s1.84");
    CHECK_STR(rows[0].objectiveText[5][2], "s1.85");
    CHECK_STR(rows[0].objectiveText[5][3], "s1.86");
    CHECK_STR(rows[0].portraitGiverName[5], "s1.87");
    CHECK_STR(rows[0].portraitGiverText[5], "s1.88");
    CHECK_STR(rows[0].portraitTurnInName[5], "s1.89");
    CHECK_STR(rows[0].portraitTurnInText[5], "s1.90");
    CHECK_STR(rows[0].title[6], "s1.91");
    CHECK_STR(rows[0].details[6], "s1.92");
    CHECK_STR(rows[0].objectives[6], "s1.93");
    CHECK_STR(rows[0].offerRewardText[6], "s1.94");
    CHECK_STR(rows[0].requestItemsText[6], "s1.95");
    CHECK_STR(rows[0].endText[6], "s1.96");
    CHECK_STR(rows[0].completedText[6], "s1.97");
    CHECK_STR(rows[0].objectiveText[6][0], "s1.98");
    CHECK_STR(rows[0].objectiveText[6][1], "s1.99");
    CHECK_STR(rows[0].objectiveText[6][2], "s1.100");
    CHECK_STR(rows[0].objectiveText[6][3], "s1.101");
    CHECK_STR(rows[0].portraitGiverName[6], "s1.102");
    CHECK_STR(rows[0].portraitGiverText[6], "s1.103");
    CHECK_STR(rows[0].portraitTurnInName[6], "s1.104");
    CHECK_STR(rows[0].portraitTurnInText[6], "s1.105");
    CHECK_STR(rows[0].title[7], "s1.106");
    CHECK_STR(rows[0].details[7], "s1.107");
    CHECK_STR(rows[0].objectives[7], "s1.108");
    CHECK_STR(rows[0].offerRewardText[7], "s1.109");
    CHECK_STR(rows[0].requestItemsText[7], "s1.110");
    CHECK_STR(rows[0].endText[7], "s1.111");
    CHECK_STR(rows[0].completedText[7], "s1.112");
    CHECK_STR(rows[0].objectiveText[7][0], "s1.113");
    CHECK_STR(rows[0].objectiveText[7][1], "s1.114");
    CHECK_STR(rows[0].objectiveText[7][2], "s1.115");
    CHECK_STR(rows[0].objectiveText[7][3], "s1.116");
    CHECK_STR(rows[0].portraitGiverName[7], "s1.117");
    CHECK_STR(rows[0].portraitGiverText[7], "s1.118");
    CHECK_STR(rows[0].portraitTurnInName[7], "s1.119");
    CHECK_STR(rows[0].portraitTurnInText[7], "s1.120");
    CHECK_STR(rows[0].title[8], "s1.121");
    CHECK_STR(rows[0].details[8], "s1.122");
    CHECK_STR(rows[0].objectives[8], "s1.123");
    CHECK_STR(rows[0].offerRewardText[8], "s1.124");
    CHECK_STR(rows[0].requestItemsText[8], "s1.125");
    CHECK_STR(rows[0].endText[8], "s1.126");
    CHECK_STR(rows[0].completedText[8], "s1.127");
    CHECK_STR(rows[0].objectiveText[8][0], "s1.128");
    CHECK_STR(rows[0].objectiveText[8][1], "s1.129");
    CHECK_STR(rows[0].objectiveText[8][2], "s1.130");
    CHECK_STR(rows[0].objectiveText[8][3], "s1.131");
    CHECK_STR(rows[0].portraitGiverName[8], "s1.132");
    CHECK_STR(rows[0].portraitGiverText[8], "s1.133");
    CHECK_STR(rows[0].portraitTurnInName[8], "s1.134");
    CHECK_STR(rows[0].portraitTurnInText[8], "s1.135");
    CHECK_STR(rows[0].title[9], "s1.136");
    CHECK_STR(rows[0].details[9], "s1.137");
    CHECK_STR(rows[0].objectives[9], "s1.138");
    CHECK_STR(rows[0].offerRewardText[9], "s1.139");
    CHECK_STR(rows[0].requestItemsText[9], "s1.140");
    CHECK_STR(rows[0].endText[9], "s1.141");
    CHECK_STR(rows[0].completedText[9], "s1.142");
    CHECK_STR(rows[0].objectiveText[9][0], "s1.143");
    CHECK_STR(rows[0].objectiveText[9][1], "s1.144");
    CHECK_STR(rows[0].objectiveText[9][2], "s1.145");
    CHECK_STR(rows[0].objectiveText[9][3], "s1.146");
    CHECK_STR(rows[0].portraitGiverName[9], "s1.147");
    CHECK_STR(rows[0].portraitGiverText[9], "s1.148");
    CHECK_STR(rows[0].portraitTurnInName[9], "s1.149");
    CHECK_STR(rows[0].portraitTurnInText[9], "s1.150");
    CHECK_STR(rows[0].title[10], "s1.151");
    CHECK_STR(rows[0].details[10], "s1.152");
    CHECK_STR(rows[0].objectives[10], "s1.153");
    CHECK_STR(rows[0].offerRewardText[10], "s1.154");
    CHECK_STR(rows[0].requestItemsText[10], "s1.155");
    CHECK_STR(rows[0].endText[10], "s1.156");
    CHECK_STR(rows[0].completedText[10], "s1.157");
    CHECK_STR(rows[0].objectiveText[10][0], "s1.158");
    CHECK_STR(rows[0].objectiveText[10][1], "s1.159");
    CHECK_STR(rows[0].objectiveText[10][2], "s1.160");
    CHECK_STR(rows[0].objectiveText[10][3], "s1.161");
    CHECK_STR(rows[0].portraitGiverName[10], "s1.162");
    CHECK_STR(rows[0].portraitGiverText[10], "s1.163");
    CHECK_STR(rows[0].portraitTurnInName[10], "s1.164");
    CHECK_STR(rows[0].portraitTurnInText[10], "s1.165");
    CHECK_EQ(rows[1].entry, 2000u);
}

// Each column of a `quest_poi` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_QuestPois)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "-1002", "1003", "1004", "1005", "1006", "1007"},
        FakeRow{"2000", "2001", "-2002", "2003", "2004", "2005", "2006", "2007"},
    });
    std::vector<QuestPoiRow> rows = QuestDataRepository::QueryQuestPois();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].questId, 1000u);
    CHECK_EQ(rows[0].poiId, 1001u);
    CHECK_EQ(rows[0].objIndex, -1002);
    CHECK_EQ(rows[0].mapId, 1003u);
    CHECK_EQ(rows[0].mapAreaId, 1004u);
    CHECK_EQ(rows[0].floorId, 1005u);
    CHECK_EQ(rows[0].unk3, 1006u);
    CHECK_EQ(rows[0].unk4, 1007u);
    CHECK_EQ(rows[1].questId, 2000u);
}

// Each column of a `quest_poi_points` row lands in its member, and the rows come back in order.
TEST(QuestDataRepository_QuestPoiPoints)
{
    Db db;
    db.query.Answer("SELECT", FakeRows{
        FakeRow{"1000", "1001", "-1002", "-1003"},
        FakeRow{"2000", "2001", "-2002", "-2003"},
    });
    std::vector<QuestPoiPointRow> rows = QuestDataRepository::QueryQuestPoiPoints();
    REQUIRE(rows.size() == 2);
    CHECK_EQ(rows[0].questId, 1000u);
    CHECK_EQ(rows[0].poiId, 1001u);
    CHECK_EQ(rows[0].x, -1002);
    CHECK_EQ(rows[0].y, -1003);
    CHECK_EQ(rows[1].questId, 2000u);
}
