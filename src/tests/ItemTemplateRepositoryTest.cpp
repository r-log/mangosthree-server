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

/// ItemTemplateRepository's read on the global WorldDatabase backed by fakes: the statement names
/// its 147 columns in table order, every column of a canned row lands in its member (a NULL text as
/// ""; the first row sets the result's width, so the short second row's texts read as NULL), and no
/// row answers an empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/items/ItemTemplateRepository.h"

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

// Each column of an `item_template` row lands in its member, and the rows come back in order.
TEST(ItemTemplateRepository_ItemTemplates)
{
    Db db;
    db.query.Answer("SELECT `entry`", FakeRows{
        FakeRow{"1000", "1001", "1002", "1003", "text 4", "1005", "1006", "1007", "1008", "9.5", "10.5", "1011", "1012", "1013", "1014", "1015", "1016", "1017", "1018", "1019", "1020", "1021", "1022", "1023", "1024", "1025", "1026", "1027", "1028", "1029", "1030", "1031", "1032", "1033", "1034", "1035", "1036", "1037", "1038", "1039", "1040", "1041", "1042", "1043", "1044", "1045", "1046", "1047", "1048", "1049", "1050", "1051", "1052", "1053", "1054", "1055", "1056", "1057", "1058", "1059", "1060", "1061", "1062", "1063", "1064", "1065", "1066", "1067", "1068", "1069", "1070", "1071", "1072", "73.5", "1074", "1075", "1076", "77.5", "1078", "1079", "1080", "1081", "1082", "1083", "84.5", "1085", "1086", "1087", "1088", "1089", "1090", "91.5", "1092", "1093", "1094", "1095", "1096", "1097", "98.5", "1099", "1100", "1101", "1102", "1103", "1104", "105.5", "1106", "1107", "1108", "1109", "text 110", "1111", "1112", "1113", "1114", "1115", "1116", "1117", "1118", "1119", "1120", "1121", "1122", "1123", "1124", "1125", "1126", "1127", "1128", "1129", "1130", "1131", "1132", "1133", "1134", "135.5", "1136", "1137", "1138", "1139", "1140", "141.5", "1142", "1143", "1144", "1145", "1146"},
        FakeRow{"2000"},
    });
    TableRead<ItemTemplateRow> read = ItemTemplateRepository::QueryItemTemplates();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `entry`, `class`, `subclass`, `unk0`, `name`, `displayid`, `Quality`, `Flags`, `Flags2`, `Unknown`, `Unknown1`, `Unknown2`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_unk1_1`, `stat_unk2_1`, `stat_type2`, `stat_value2`, `stat_unk1_2`, `stat_unk2_2`, `stat_type3`, `stat_value3`, `stat_unk1_3`, `stat_unk2_3`, `stat_type4`, `stat_value4`, `stat_unk1_4`, `stat_unk2_4`, `stat_type5`, `stat_value5`, `stat_unk1_5`, `stat_unk2_5`, `stat_type6`, `stat_value6`, `stat_unk1_6`, `stat_unk2_6`, `stat_type7`, `stat_value7`, `stat_unk1_7`, `stat_unk2_7`, `stat_type8`, `stat_value8`, `stat_unk1_8`, `stat_unk2_8`, `stat_type9`, `stat_value9`, `stat_unk1_9`, `stat_unk2_9`, `stat_type10`, `stat_value10`, `stat_unk1_10`, `stat_unk2_10`, `ScalingStatDistribution`, `DamageType`, `delay`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `Duration`, `ItemLimitCategory`, `HolidayId`, `DisenchantID`, `FoodType`, `StatScalingFactor`, `Unknown400_1`, `Unknown400_2`, `minMoneyLoot`, `maxMoneyLoot`, `ExtraFlags` FROM `item_template`");
    REQUIRE(read.rows.size() == 2);
    ItemTemplateRow const& row = read.rows[0];
    CHECK_EQ(row.entry, 1000u);
    CHECK_EQ(row.itemClass, 1001u);
    CHECK_EQ(row.subClass, 1002u);
    CHECK_EQ(row.unk0, 1003u);
    CHECK_STR(row.name, "text 4");
    CHECK_EQ(row.displayId, 1005u);
    CHECK_EQ(row.quality, 1006u);
    CHECK_EQ(row.flags, 1007u);
    CHECK_EQ(row.flags2, 1008u);
    CHECK(row.unknown == 9.5f);
    CHECK(row.unknown1 == 10.5f);
    CHECK_EQ(row.unknown2, 1011u);
    CHECK_EQ(row.buyCount, 1012u);
    CHECK_EQ(row.buyPrice, 1013u);
    CHECK_EQ(row.sellPrice, 1014u);
    CHECK_EQ(row.inventoryType, 1015u);
    CHECK_EQ(row.allowableClass, 1016u);
    CHECK_EQ(row.allowableRace, 1017u);
    CHECK_EQ(row.itemLevel, 1018u);
    CHECK_EQ(row.requiredLevel, 1019u);
    CHECK_EQ(row.requiredSkill, 1020u);
    CHECK_EQ(row.requiredSkillRank, 1021u);
    CHECK_EQ(row.requiredSpell, 1022u);
    CHECK_EQ(row.requiredHonorRank, 1023u);
    CHECK_EQ(row.requiredCityRank, 1024u);
    CHECK_EQ(row.requiredReputationFaction, 1025u);
    CHECK_EQ(row.requiredReputationRank, 1026u);
    CHECK_EQ(row.maxCount, 1027u);
    CHECK_EQ(row.stackable, 1028u);
    CHECK_EQ(row.containerSlots, 1029u);
    CHECK_EQ(row.stats[0].type, 1030u);
    CHECK_EQ(row.stats[0].value, 1031u);
    CHECK_EQ(row.stats[0].unk1, 1032u);
    CHECK_EQ(row.stats[0].unk2, 1033u);
    CHECK_EQ(row.stats[1].type, 1034u);
    CHECK_EQ(row.stats[1].value, 1035u);
    CHECK_EQ(row.stats[1].unk1, 1036u);
    CHECK_EQ(row.stats[1].unk2, 1037u);
    CHECK_EQ(row.stats[2].type, 1038u);
    CHECK_EQ(row.stats[2].value, 1039u);
    CHECK_EQ(row.stats[2].unk1, 1040u);
    CHECK_EQ(row.stats[2].unk2, 1041u);
    CHECK_EQ(row.stats[3].type, 1042u);
    CHECK_EQ(row.stats[3].value, 1043u);
    CHECK_EQ(row.stats[3].unk1, 1044u);
    CHECK_EQ(row.stats[3].unk2, 1045u);
    CHECK_EQ(row.stats[4].type, 1046u);
    CHECK_EQ(row.stats[4].value, 1047u);
    CHECK_EQ(row.stats[4].unk1, 1048u);
    CHECK_EQ(row.stats[4].unk2, 1049u);
    CHECK_EQ(row.stats[5].type, 1050u);
    CHECK_EQ(row.stats[5].value, 1051u);
    CHECK_EQ(row.stats[5].unk1, 1052u);
    CHECK_EQ(row.stats[5].unk2, 1053u);
    CHECK_EQ(row.stats[6].type, 1054u);
    CHECK_EQ(row.stats[6].value, 1055u);
    CHECK_EQ(row.stats[6].unk1, 1056u);
    CHECK_EQ(row.stats[6].unk2, 1057u);
    CHECK_EQ(row.stats[7].type, 1058u);
    CHECK_EQ(row.stats[7].value, 1059u);
    CHECK_EQ(row.stats[7].unk1, 1060u);
    CHECK_EQ(row.stats[7].unk2, 1061u);
    CHECK_EQ(row.stats[8].type, 1062u);
    CHECK_EQ(row.stats[8].value, 1063u);
    CHECK_EQ(row.stats[8].unk1, 1064u);
    CHECK_EQ(row.stats[8].unk2, 1065u);
    CHECK_EQ(row.stats[9].type, 1066u);
    CHECK_EQ(row.stats[9].value, 1067u);
    CHECK_EQ(row.stats[9].unk1, 1068u);
    CHECK_EQ(row.stats[9].unk2, 1069u);
    CHECK_EQ(row.scalingStatDistribution, 1070u);
    CHECK_EQ(row.damageType, 1071u);
    CHECK_EQ(row.delay, 1072u);
    CHECK(row.rangedModRange == 73.5f);
    CHECK_EQ(row.spells[0].id, 1074u);
    CHECK_EQ(row.spells[0].trigger, 1075u);
    CHECK_EQ(row.spells[0].charges, 1076u);
    CHECK(row.spells[0].ppmRate == 77.5f);
    CHECK_EQ(row.spells[0].cooldown, 1078u);
    CHECK_EQ(row.spells[0].category, 1079u);
    CHECK_EQ(row.spells[0].categoryCooldown, 1080u);
    CHECK_EQ(row.spells[1].id, 1081u);
    CHECK_EQ(row.spells[1].trigger, 1082u);
    CHECK_EQ(row.spells[1].charges, 1083u);
    CHECK(row.spells[1].ppmRate == 84.5f);
    CHECK_EQ(row.spells[1].cooldown, 1085u);
    CHECK_EQ(row.spells[1].category, 1086u);
    CHECK_EQ(row.spells[1].categoryCooldown, 1087u);
    CHECK_EQ(row.spells[2].id, 1088u);
    CHECK_EQ(row.spells[2].trigger, 1089u);
    CHECK_EQ(row.spells[2].charges, 1090u);
    CHECK(row.spells[2].ppmRate == 91.5f);
    CHECK_EQ(row.spells[2].cooldown, 1092u);
    CHECK_EQ(row.spells[2].category, 1093u);
    CHECK_EQ(row.spells[2].categoryCooldown, 1094u);
    CHECK_EQ(row.spells[3].id, 1095u);
    CHECK_EQ(row.spells[3].trigger, 1096u);
    CHECK_EQ(row.spells[3].charges, 1097u);
    CHECK(row.spells[3].ppmRate == 98.5f);
    CHECK_EQ(row.spells[3].cooldown, 1099u);
    CHECK_EQ(row.spells[3].category, 1100u);
    CHECK_EQ(row.spells[3].categoryCooldown, 1101u);
    CHECK_EQ(row.spells[4].id, 1102u);
    CHECK_EQ(row.spells[4].trigger, 1103u);
    CHECK_EQ(row.spells[4].charges, 1104u);
    CHECK(row.spells[4].ppmRate == 105.5f);
    CHECK_EQ(row.spells[4].cooldown, 1106u);
    CHECK_EQ(row.spells[4].category, 1107u);
    CHECK_EQ(row.spells[4].categoryCooldown, 1108u);
    CHECK_EQ(row.bonding, 1109u);
    CHECK_STR(row.description, "text 110");
    CHECK_EQ(row.pageText, 1111u);
    CHECK_EQ(row.languageId, 1112u);
    CHECK_EQ(row.pageMaterial, 1113u);
    CHECK_EQ(row.startQuest, 1114u);
    CHECK_EQ(row.lockId, 1115u);
    CHECK_EQ(row.material, 1116u);
    CHECK_EQ(row.sheath, 1117u);
    CHECK_EQ(row.randomProperty, 1118u);
    CHECK_EQ(row.randomSuffix, 1119u);
    CHECK_EQ(row.itemSet, 1120u);
    CHECK_EQ(row.maxDurability, 1121u);
    CHECK_EQ(row.area, 1122u);
    CHECK_EQ(row.map, 1123u);
    CHECK_EQ(row.bagFamily, 1124u);
    CHECK_EQ(row.totemCategory, 1125u);
    CHECK_EQ(row.sockets[0].color, 1126u);
    CHECK_EQ(row.sockets[0].content, 1127u);
    CHECK_EQ(row.sockets[1].color, 1128u);
    CHECK_EQ(row.sockets[1].content, 1129u);
    CHECK_EQ(row.sockets[2].color, 1130u);
    CHECK_EQ(row.sockets[2].content, 1131u);
    CHECK_EQ(row.socketBonus, 1132u);
    CHECK_EQ(row.gemProperties, 1133u);
    CHECK_EQ(row.requiredDisenchantSkill, 1134u);
    CHECK(row.armorDamageModifier == 135.5f);
    CHECK_EQ(row.duration, 1136u);
    CHECK_EQ(row.itemLimitCategory, 1137u);
    CHECK_EQ(row.holidayId, 1138u);
    CHECK_EQ(row.disenchantId, 1139u);
    CHECK_EQ(row.foodType, 1140u);
    CHECK(row.statScalingFactor == 141.5f);
    CHECK_EQ(row.unknown400_1, 1142u);
    CHECK_EQ(row.unknown400_2, 1143u);
    CHECK_EQ(row.minMoneyLoot, 1144u);
    CHECK_EQ(row.maxMoneyLoot, 1145u);
    CHECK_EQ(row.extraFlags, 1146u);
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_STR(read.rows[1].name, "");
    CHECK_STR(read.rows[1].description, "");
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(ItemTemplateRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<ItemTemplateRow> read = ItemTemplateRepository::QueryItemTemplates();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `entry`, `class`, `subclass`, `unk0`, `name`, `displayid`, `Quality`, `Flags`, `Flags2`, `Unknown`, `Unknown1`, `Unknown2`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_unk1_1`, `stat_unk2_1`, `stat_type2`, `stat_value2`, `stat_unk1_2`, `stat_unk2_2`, `stat_type3`, `stat_value3`, `stat_unk1_3`, `stat_unk2_3`, `stat_type4`, `stat_value4`, `stat_unk1_4`, `stat_unk2_4`, `stat_type5`, `stat_value5`, `stat_unk1_5`, `stat_unk2_5`, `stat_type6`, `stat_value6`, `stat_unk1_6`, `stat_unk2_6`, `stat_type7`, `stat_value7`, `stat_unk1_7`, `stat_unk2_7`, `stat_type8`, `stat_value8`, `stat_unk1_8`, `stat_unk2_8`, `stat_type9`, `stat_value9`, `stat_unk1_9`, `stat_unk2_9`, `stat_type10`, `stat_value10`, `stat_unk1_10`, `stat_unk2_10`, `ScalingStatDistribution`, `DamageType`, `delay`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `Duration`, `ItemLimitCategory`, `HolidayId`, `DisenchantID`, `FoodType`, `StatScalingFactor`, `Unknown400_1`, `Unknown400_2`, `minMoneyLoot`, `maxMoneyLoot`, `ExtraFlags` FROM `item_template`) AS probe");
    }
    {
        Db db;
        TableRead<ItemTemplateRow> read = ItemTemplateRepository::QueryItemTemplates();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
