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

#include "CreatureTemplateRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace CreatureTemplateRepository
{
    namespace
    {
        std::string ReadText(Field const& field)
        {
            char const* text = field.GetString();
            return text ? text : "";
        }
    }

    CreatureTemplateRow ReadRow(Field* fields)
    {
        CreatureTemplateRow row;
        row.entry = fields[0].GetUInt32();
        row.name = ReadText(fields[1]);
        row.subName = ReadText(fields[2]);
        row.iconName = ReadText(fields[3]);
        row.minLevel = fields[4].GetUInt32();
        row.maxLevel = fields[5].GetUInt32();
        row.difficultyEntry[0] = fields[6].GetUInt32();
        row.difficultyEntry[1] = fields[7].GetUInt32();
        row.difficultyEntry[2] = fields[8].GetUInt32();
        row.modelId[0] = fields[9].GetUInt32();
        row.modelId[1] = fields[10].GetUInt32();
        row.modelId[2] = fields[11].GetUInt32();
        row.modelId[3] = fields[12].GetUInt32();
        row.factionAlliance = fields[13].GetUInt32();
        row.factionHorde = fields[14].GetUInt32();
        row.scale = fields[15].GetFloat();
        row.family = fields[16].GetUInt32();
        row.creatureType = fields[17].GetUInt32();
        row.inhabitType = fields[18].GetUInt32();
        row.regenerateStats = fields[19].GetUInt32();
        row.racialLeader = fields[20].GetUInt32();
        row.npcFlags = fields[21].GetUInt32();
        row.unitFlags = fields[22].GetUInt32();
        row.unitFlags2 = fields[23].GetUInt32();
        row.dynamicFlags = fields[24].GetUInt32();
        row.extraFlags = fields[25].GetUInt32();
        row.creatureTypeFlags = fields[26].GetUInt32();
        row.speedWalk = fields[27].GetFloat();
        row.speedRun = fields[28].GetFloat();
        row.unitClass = fields[29].GetUInt32();
        row.rank = fields[30].GetUInt32();
        row.expansion = fields[31].GetUInt32();
        row.healthMultiplier = fields[32].GetFloat();
        row.powerMultiplier = fields[33].GetFloat();
        row.damageMultiplier = fields[34].GetFloat();
        row.damageVariance = fields[35].GetFloat();
        row.armorMultiplier = fields[36].GetFloat();
        row.experienceMultiplier = fields[37].GetFloat();
        row.minLevelHealth = fields[38].GetUInt32();
        row.maxLevelHealth = fields[39].GetUInt32();
        row.minLevelMana = fields[40].GetUInt32();
        row.maxLevelMana = fields[41].GetUInt32();
        row.minMeleeDmg = fields[42].GetFloat();
        row.maxMeleeDmg = fields[43].GetFloat();
        row.minRangedDmg = fields[44].GetFloat();
        row.maxRangedDmg = fields[45].GetFloat();
        row.armor = fields[46].GetUInt32();
        row.meleeAttackPower = fields[47].GetUInt32();
        row.rangedAttackPower = fields[48].GetUInt32();
        row.meleeBaseAttackTime = fields[49].GetUInt32();
        row.rangedBaseAttackTime = fields[50].GetUInt32();
        row.damageSchool = fields[51].GetUInt32();
        row.minLootGold = fields[52].GetUInt32();
        row.maxLootGold = fields[53].GetUInt32();
        row.lootId = fields[54].GetUInt32();
        row.pickpocketLootId = fields[55].GetUInt32();
        row.skinningLootId = fields[56].GetUInt32();
        row.killCredit[0] = fields[57].GetUInt32();
        row.killCredit[1] = fields[58].GetUInt32();
        row.questItems[0] = fields[59].GetUInt32();
        row.questItems[1] = fields[60].GetUInt32();
        row.questItems[2] = fields[61].GetUInt32();
        row.questItems[3] = fields[62].GetUInt32();
        row.questItems[4] = fields[63].GetUInt32();
        row.questItems[5] = fields[64].GetUInt32();
        row.mechanicImmuneMask = fields[65].GetUInt32();
        row.resistanceHoly = fields[66].GetUInt32();
        row.resistanceFire = fields[67].GetUInt32();
        row.resistanceNature = fields[68].GetUInt32();
        row.resistanceFrost = fields[69].GetUInt32();
        row.resistanceShadow = fields[70].GetUInt32();
        row.resistanceArcane = fields[71].GetUInt32();
        row.petSpellDataId = fields[72].GetUInt32();
        row.movementType = fields[73].GetUInt32();
        row.movementTemplateId = fields[74].GetUInt32();
        row.trainerType = fields[75].GetUInt32();
        row.trainerSpell = fields[76].GetUInt32();
        row.trainerClass = fields[77].GetUInt32();
        row.trainerRace = fields[78].GetUInt32();
        row.trainerTemplateId = fields[79].GetUInt32();
        row.vendorTemplateId = fields[80].GetUInt32();
        row.equipmentTemplateId = fields[81].GetUInt32();
        row.vehicleTemplateId = fields[82].GetUInt32();
        row.gossipMenuId = fields[83].GetUInt32();
        row.aiName = ReadText(fields[84]);
        return row;
    }

    TableRead<CreatureTemplateRow> QueryCreatureTemplates()
    {
        // the columns in table order, 0 to 84
        return ReadWorldTable<CreatureTemplateRow>("SELECT `Entry`, `Name`, `SubName`, `IconName`, `MinLevel`, `MaxLevel`, `DifficultyEntry1`, `DifficultyEntry2`, `DifficultyEntry3`, `ModelId1`, `ModelId2`, `ModelId3`, `ModelId4`, `FactionAlliance`, `FactionHorde`, `Scale`, `Family`, `CreatureType`, `InhabitType`, `RegenerateStats`, `RacialLeader`, `NpcFlags`, `UnitFlags`, `UnitFlags2`, `DynamicFlags`, `ExtraFlags`, `CreatureTypeFlags`, `SpeedWalk`, `SpeedRun`, `UnitClass`, `Rank`, `Expansion`, `HealthMultiplier`, `PowerMultiplier`, `DamageMultiplier`, `DamageVariance`, `ArmorMultiplier`, `ExperienceMultiplier`, `MinLevelHealth`, `MaxLevelHealth`, `MinLevelMana`, `MaxLevelMana`, `MinMeleeDmg`, `MaxMeleeDmg`, `MinRangedDmg`, `MaxRangedDmg`, `Armor`, `MeleeAttackPower`, `RangedAttackPower`, `MeleeBaseAttackTime`, `RangedBaseAttackTime`, `DamageSchool`, `MinLootGold`, `MaxLootGold`, `LootId`, `PickpocketLootId`, `SkinningLootId`, `KillCredit1`, `KillCredit2`, `QuestItem1`, `QuestItem2`, `QuestItem3`, `QuestItem4`, `QuestItem5`, `QuestItem6`, `MechanicImmuneMask`, `ResistanceHoly`, `ResistanceFire`, `ResistanceNature`, `ResistanceFrost`, `ResistanceShadow`, `ResistanceArcane`, `PetSpellDataId`, `MovementType`, `MovementTemplateId`, `TrainerType`, `TrainerSpell`, `TrainerClass`, `TrainerRace`, `TrainerTemplateId`, `VendorTemplateId`, `EquipmentTemplateId`, `VehicleTemplateId`, `GossipMenuId`, `AIName` FROM `creature_template`", ReadRow);
    }
}
