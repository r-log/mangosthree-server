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

/// CreatureTemplateRepository's read on the global WorldDatabase backed by fakes: the statement names
/// its 85 columns in table order, every column of a canned row lands in its member (a NULL text as "";
/// the first row sets the result's width, so the short second row's texts read as NULL), and no row
/// answers an empty table or an unreadable one, as the count of the same select says.

#include "TestHarness.h"
#include "FakeDatabase.h"
#include "Database/DatabaseEnv.h"
#include "persistence/world/creatures/CreatureTemplateRepository.h"

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

// Each column of a `creature_template` row lands in its member, and the rows come back in order.
TEST(CreatureTemplateRepository_CreatureTemplates)
{
    Db db;
    db.query.Answer("SELECT `Entry`", FakeRows{
        FakeRow{"1000", "text 1", "text 2", "text 3", "1004", "1005", "1006", "1007", "1008", "1009", "1010", "1011", "1012", "1013", "1014", "15.5", "1016", "1017", "1018", "1019", "1020", "1021", "1022", "1023", "1024", "1025", "1026", "27.5", "28.5", "1029", "1030", "1031", "32.5", "33.5", "34.5", "35.5", "36.5", "37.5", "1038", "1039", "1040", "1041", "42.5", "43.5", "44.5", "45.5", "1046", "1047", "1048", "1049", "1050", "1051", "1052", "1053", "1054", "1055", "1056", "1057", "1058", "1059", "1060", "1061", "1062", "1063", "1064", "1065", "1066", "1067", "1068", "1069", "1070", "1071", "1072", "1073", "1074", "1075", "1076", "1077", "1078", "1079", "1080", "1081", "1082", "1083", "text 84"},
        FakeRow{"2000"},
    });
    TableRead<CreatureTemplateRow> read = CreatureTemplateRepository::QueryCreatureTemplates();
    CHECK(read.status == TableReadStatus::Loaded);
    REQUIRE(db.query.executed.size() == 1);
    CHECK_STR(db.query.executed[0], "SELECT `Entry`, `Name`, `SubName`, `IconName`, `MinLevel`, `MaxLevel`, `DifficultyEntry1`, `DifficultyEntry2`, `DifficultyEntry3`, `ModelId1`, `ModelId2`, `ModelId3`, `ModelId4`, `FactionAlliance`, `FactionHorde`, `Scale`, `Family`, `CreatureType`, `InhabitType`, `RegenerateStats`, `RacialLeader`, `NpcFlags`, `UnitFlags`, `UnitFlags2`, `DynamicFlags`, `ExtraFlags`, `CreatureTypeFlags`, `SpeedWalk`, `SpeedRun`, `UnitClass`, `Rank`, `Expansion`, `HealthMultiplier`, `PowerMultiplier`, `DamageMultiplier`, `DamageVariance`, `ArmorMultiplier`, `ExperienceMultiplier`, `MinLevelHealth`, `MaxLevelHealth`, `MinLevelMana`, `MaxLevelMana`, `MinMeleeDmg`, `MaxMeleeDmg`, `MinRangedDmg`, `MaxRangedDmg`, `Armor`, `MeleeAttackPower`, `RangedAttackPower`, `MeleeBaseAttackTime`, `RangedBaseAttackTime`, `DamageSchool`, `MinLootGold`, `MaxLootGold`, `LootId`, `PickpocketLootId`, `SkinningLootId`, `KillCredit1`, `KillCredit2`, `QuestItem1`, `QuestItem2`, `QuestItem3`, `QuestItem4`, `QuestItem5`, `QuestItem6`, `MechanicImmuneMask`, `ResistanceHoly`, `ResistanceFire`, `ResistanceNature`, `ResistanceFrost`, `ResistanceShadow`, `ResistanceArcane`, `PetSpellDataId`, `MovementType`, `MovementTemplateId`, `TrainerType`, `TrainerSpell`, `TrainerClass`, `TrainerRace`, `TrainerTemplateId`, `VendorTemplateId`, `EquipmentTemplateId`, `VehicleTemplateId`, `GossipMenuId`, `AIName` FROM `creature_template`");
    REQUIRE(read.rows.size() == 2);
    CreatureTemplateRow const& row = read.rows[0];
    CHECK_EQ(row.entry, 1000u);
    CHECK_STR(row.name, "text 1");
    CHECK_STR(row.subName, "text 2");
    CHECK_STR(row.iconName, "text 3");
    CHECK_EQ(row.minLevel, 1004u);
    CHECK_EQ(row.maxLevel, 1005u);
    CHECK_EQ(row.difficultyEntry[0], 1006u);
    CHECK_EQ(row.difficultyEntry[1], 1007u);
    CHECK_EQ(row.difficultyEntry[2], 1008u);
    CHECK_EQ(row.modelId[0], 1009u);
    CHECK_EQ(row.modelId[1], 1010u);
    CHECK_EQ(row.modelId[2], 1011u);
    CHECK_EQ(row.modelId[3], 1012u);
    CHECK_EQ(row.factionAlliance, 1013u);
    CHECK_EQ(row.factionHorde, 1014u);
    CHECK(row.scale == 15.5f);
    CHECK_EQ(row.family, 1016u);
    CHECK_EQ(row.creatureType, 1017u);
    CHECK_EQ(row.inhabitType, 1018u);
    CHECK_EQ(row.regenerateStats, 1019u);
    CHECK_EQ(row.racialLeader, 1020u);
    CHECK_EQ(row.npcFlags, 1021u);
    CHECK_EQ(row.unitFlags, 1022u);
    CHECK_EQ(row.unitFlags2, 1023u);
    CHECK_EQ(row.dynamicFlags, 1024u);
    CHECK_EQ(row.extraFlags, 1025u);
    CHECK_EQ(row.creatureTypeFlags, 1026u);
    CHECK(row.speedWalk == 27.5f);
    CHECK(row.speedRun == 28.5f);
    CHECK_EQ(row.unitClass, 1029u);
    CHECK_EQ(row.rank, 1030u);
    CHECK_EQ(row.expansion, 1031u);
    CHECK(row.healthMultiplier == 32.5f);
    CHECK(row.powerMultiplier == 33.5f);
    CHECK(row.damageMultiplier == 34.5f);
    CHECK(row.damageVariance == 35.5f);
    CHECK(row.armorMultiplier == 36.5f);
    CHECK(row.experienceMultiplier == 37.5f);
    CHECK_EQ(row.minLevelHealth, 1038u);
    CHECK_EQ(row.maxLevelHealth, 1039u);
    CHECK_EQ(row.minLevelMana, 1040u);
    CHECK_EQ(row.maxLevelMana, 1041u);
    CHECK(row.minMeleeDmg == 42.5f);
    CHECK(row.maxMeleeDmg == 43.5f);
    CHECK(row.minRangedDmg == 44.5f);
    CHECK(row.maxRangedDmg == 45.5f);
    CHECK_EQ(row.armor, 1046u);
    CHECK_EQ(row.meleeAttackPower, 1047u);
    CHECK_EQ(row.rangedAttackPower, 1048u);
    CHECK_EQ(row.meleeBaseAttackTime, 1049u);
    CHECK_EQ(row.rangedBaseAttackTime, 1050u);
    CHECK_EQ(row.damageSchool, 1051u);
    CHECK_EQ(row.minLootGold, 1052u);
    CHECK_EQ(row.maxLootGold, 1053u);
    CHECK_EQ(row.lootId, 1054u);
    CHECK_EQ(row.pickpocketLootId, 1055u);
    CHECK_EQ(row.skinningLootId, 1056u);
    CHECK_EQ(row.killCredit[0], 1057u);
    CHECK_EQ(row.killCredit[1], 1058u);
    CHECK_EQ(row.questItems[0], 1059u);
    CHECK_EQ(row.questItems[1], 1060u);
    CHECK_EQ(row.questItems[2], 1061u);
    CHECK_EQ(row.questItems[3], 1062u);
    CHECK_EQ(row.questItems[4], 1063u);
    CHECK_EQ(row.questItems[5], 1064u);
    CHECK_EQ(row.mechanicImmuneMask, 1065u);
    CHECK_EQ(row.resistanceHoly, 1066u);
    CHECK_EQ(row.resistanceFire, 1067u);
    CHECK_EQ(row.resistanceNature, 1068u);
    CHECK_EQ(row.resistanceFrost, 1069u);
    CHECK_EQ(row.resistanceShadow, 1070u);
    CHECK_EQ(row.resistanceArcane, 1071u);
    CHECK_EQ(row.petSpellDataId, 1072u);
    CHECK_EQ(row.movementType, 1073u);
    CHECK_EQ(row.movementTemplateId, 1074u);
    CHECK_EQ(row.trainerType, 1075u);
    CHECK_EQ(row.trainerSpell, 1076u);
    CHECK_EQ(row.trainerClass, 1077u);
    CHECK_EQ(row.trainerRace, 1078u);
    CHECK_EQ(row.trainerTemplateId, 1079u);
    CHECK_EQ(row.vendorTemplateId, 1080u);
    CHECK_EQ(row.equipmentTemplateId, 1081u);
    CHECK_EQ(row.vehicleTemplateId, 1082u);
    CHECK_EQ(row.gossipMenuId, 1083u);
    CHECK_STR(row.aiName, "text 84");
    CHECK_EQ(read.rows[1].entry, 2000u);
    CHECK_STR(read.rows[1].name, "");
    CHECK_STR(read.rows[1].subName, "");
    CHECK_STR(read.rows[1].aiName, "");
}

// No row and a count of 0 answer an empty table; no row and no count an unreadable one.
TEST(CreatureTemplateRepository_NoRowsAnswerEmptyOrUnreadable)
{
    {
        Db db;
        db.query.Answer("SELECT COUNT(*)", FakeRows{ FakeRow{"0"} });
        TableRead<CreatureTemplateRow> read = CreatureTemplateRepository::QueryCreatureTemplates();
        CHECK(read.status == TableReadStatus::Empty);
        REQUIRE(db.query.executed.size() == 2);
        CHECK_STR(db.query.executed[1], "SELECT COUNT(*) FROM (SELECT `Entry`, `Name`, `SubName`, `IconName`, `MinLevel`, `MaxLevel`, `DifficultyEntry1`, `DifficultyEntry2`, `DifficultyEntry3`, `ModelId1`, `ModelId2`, `ModelId3`, `ModelId4`, `FactionAlliance`, `FactionHorde`, `Scale`, `Family`, `CreatureType`, `InhabitType`, `RegenerateStats`, `RacialLeader`, `NpcFlags`, `UnitFlags`, `UnitFlags2`, `DynamicFlags`, `ExtraFlags`, `CreatureTypeFlags`, `SpeedWalk`, `SpeedRun`, `UnitClass`, `Rank`, `Expansion`, `HealthMultiplier`, `PowerMultiplier`, `DamageMultiplier`, `DamageVariance`, `ArmorMultiplier`, `ExperienceMultiplier`, `MinLevelHealth`, `MaxLevelHealth`, `MinLevelMana`, `MaxLevelMana`, `MinMeleeDmg`, `MaxMeleeDmg`, `MinRangedDmg`, `MaxRangedDmg`, `Armor`, `MeleeAttackPower`, `RangedAttackPower`, `MeleeBaseAttackTime`, `RangedBaseAttackTime`, `DamageSchool`, `MinLootGold`, `MaxLootGold`, `LootId`, `PickpocketLootId`, `SkinningLootId`, `KillCredit1`, `KillCredit2`, `QuestItem1`, `QuestItem2`, `QuestItem3`, `QuestItem4`, `QuestItem5`, `QuestItem6`, `MechanicImmuneMask`, `ResistanceHoly`, `ResistanceFire`, `ResistanceNature`, `ResistanceFrost`, `ResistanceShadow`, `ResistanceArcane`, `PetSpellDataId`, `MovementType`, `MovementTemplateId`, `TrainerType`, `TrainerSpell`, `TrainerClass`, `TrainerRace`, `TrainerTemplateId`, `VendorTemplateId`, `EquipmentTemplateId`, `VehicleTemplateId`, `GossipMenuId`, `AIName` FROM `creature_template`) AS probe");
    }
    {
        Db db;
        TableRead<CreatureTemplateRow> read = CreatureTemplateRepository::QueryCreatureTemplates();
        CHECK(read.status == TableReadStatus::QueryFailed);
        CHECK(read.Unreadable());
    }
}
