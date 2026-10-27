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

#include "ItemTemplateRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace ItemTemplateRepository
{
    namespace
    {
        std::string ReadText(Field const& field)
        {
            char const* text = field.GetString();
            return text ? text : "";
        }
    }

    ItemTemplateRow ReadRow(Field* fields)
    {
        ItemTemplateRow row;
        row.entry = fields[0].GetUInt32();
        row.itemClass = fields[1].GetUInt32();
        row.subClass = fields[2].GetUInt32();
        row.unk0 = fields[3].GetUInt32();
        row.name = ReadText(fields[4]);
        row.displayId = fields[5].GetUInt32();
        row.quality = fields[6].GetUInt32();
        row.flags = fields[7].GetUInt32();
        row.flags2 = fields[8].GetUInt32();
        row.unknown = fields[9].GetFloat();
        row.unknown1 = fields[10].GetFloat();
        row.unknown2 = fields[11].GetUInt32();
        row.buyCount = fields[12].GetUInt32();
        row.buyPrice = fields[13].GetUInt32();
        row.sellPrice = fields[14].GetUInt32();
        row.inventoryType = fields[15].GetUInt32();
        row.allowableClass = fields[16].GetUInt32();
        row.allowableRace = fields[17].GetUInt32();
        row.itemLevel = fields[18].GetUInt32();
        row.requiredLevel = fields[19].GetUInt32();
        row.requiredSkill = fields[20].GetUInt32();
        row.requiredSkillRank = fields[21].GetUInt32();
        row.requiredSpell = fields[22].GetUInt32();
        row.requiredHonorRank = fields[23].GetUInt32();
        row.requiredCityRank = fields[24].GetUInt32();
        row.requiredReputationFaction = fields[25].GetUInt32();
        row.requiredReputationRank = fields[26].GetUInt32();
        row.maxCount = fields[27].GetUInt32();
        row.stackable = fields[28].GetUInt32();
        row.containerSlots = fields[29].GetUInt32();
        row.stats[0].type = fields[30].GetUInt32();
        row.stats[0].value = fields[31].GetUInt32();
        row.stats[0].unk1 = fields[32].GetUInt32();
        row.stats[0].unk2 = fields[33].GetUInt32();
        row.stats[1].type = fields[34].GetUInt32();
        row.stats[1].value = fields[35].GetUInt32();
        row.stats[1].unk1 = fields[36].GetUInt32();
        row.stats[1].unk2 = fields[37].GetUInt32();
        row.stats[2].type = fields[38].GetUInt32();
        row.stats[2].value = fields[39].GetUInt32();
        row.stats[2].unk1 = fields[40].GetUInt32();
        row.stats[2].unk2 = fields[41].GetUInt32();
        row.stats[3].type = fields[42].GetUInt32();
        row.stats[3].value = fields[43].GetUInt32();
        row.stats[3].unk1 = fields[44].GetUInt32();
        row.stats[3].unk2 = fields[45].GetUInt32();
        row.stats[4].type = fields[46].GetUInt32();
        row.stats[4].value = fields[47].GetUInt32();
        row.stats[4].unk1 = fields[48].GetUInt32();
        row.stats[4].unk2 = fields[49].GetUInt32();
        row.stats[5].type = fields[50].GetUInt32();
        row.stats[5].value = fields[51].GetUInt32();
        row.stats[5].unk1 = fields[52].GetUInt32();
        row.stats[5].unk2 = fields[53].GetUInt32();
        row.stats[6].type = fields[54].GetUInt32();
        row.stats[6].value = fields[55].GetUInt32();
        row.stats[6].unk1 = fields[56].GetUInt32();
        row.stats[6].unk2 = fields[57].GetUInt32();
        row.stats[7].type = fields[58].GetUInt32();
        row.stats[7].value = fields[59].GetUInt32();
        row.stats[7].unk1 = fields[60].GetUInt32();
        row.stats[7].unk2 = fields[61].GetUInt32();
        row.stats[8].type = fields[62].GetUInt32();
        row.stats[8].value = fields[63].GetUInt32();
        row.stats[8].unk1 = fields[64].GetUInt32();
        row.stats[8].unk2 = fields[65].GetUInt32();
        row.stats[9].type = fields[66].GetUInt32();
        row.stats[9].value = fields[67].GetUInt32();
        row.stats[9].unk1 = fields[68].GetUInt32();
        row.stats[9].unk2 = fields[69].GetUInt32();
        row.scalingStatDistribution = fields[70].GetUInt32();
        row.damageType = fields[71].GetUInt32();
        row.delay = fields[72].GetUInt32();
        row.rangedModRange = fields[73].GetFloat();
        row.spells[0].id = fields[74].GetUInt32();
        row.spells[0].trigger = fields[75].GetUInt32();
        row.spells[0].charges = fields[76].GetUInt32();
        row.spells[0].ppmRate = fields[77].GetFloat();
        row.spells[0].cooldown = fields[78].GetUInt32();
        row.spells[0].category = fields[79].GetUInt32();
        row.spells[0].categoryCooldown = fields[80].GetUInt32();
        row.spells[1].id = fields[81].GetUInt32();
        row.spells[1].trigger = fields[82].GetUInt32();
        row.spells[1].charges = fields[83].GetUInt32();
        row.spells[1].ppmRate = fields[84].GetFloat();
        row.spells[1].cooldown = fields[85].GetUInt32();
        row.spells[1].category = fields[86].GetUInt32();
        row.spells[1].categoryCooldown = fields[87].GetUInt32();
        row.spells[2].id = fields[88].GetUInt32();
        row.spells[2].trigger = fields[89].GetUInt32();
        row.spells[2].charges = fields[90].GetUInt32();
        row.spells[2].ppmRate = fields[91].GetFloat();
        row.spells[2].cooldown = fields[92].GetUInt32();
        row.spells[2].category = fields[93].GetUInt32();
        row.spells[2].categoryCooldown = fields[94].GetUInt32();
        row.spells[3].id = fields[95].GetUInt32();
        row.spells[3].trigger = fields[96].GetUInt32();
        row.spells[3].charges = fields[97].GetUInt32();
        row.spells[3].ppmRate = fields[98].GetFloat();
        row.spells[3].cooldown = fields[99].GetUInt32();
        row.spells[3].category = fields[100].GetUInt32();
        row.spells[3].categoryCooldown = fields[101].GetUInt32();
        row.spells[4].id = fields[102].GetUInt32();
        row.spells[4].trigger = fields[103].GetUInt32();
        row.spells[4].charges = fields[104].GetUInt32();
        row.spells[4].ppmRate = fields[105].GetFloat();
        row.spells[4].cooldown = fields[106].GetUInt32();
        row.spells[4].category = fields[107].GetUInt32();
        row.spells[4].categoryCooldown = fields[108].GetUInt32();
        row.bonding = fields[109].GetUInt32();
        row.description = ReadText(fields[110]);
        row.pageText = fields[111].GetUInt32();
        row.languageId = fields[112].GetUInt32();
        row.pageMaterial = fields[113].GetUInt32();
        row.startQuest = fields[114].GetUInt32();
        row.lockId = fields[115].GetUInt32();
        row.material = fields[116].GetUInt32();
        row.sheath = fields[117].GetUInt32();
        row.randomProperty = fields[118].GetUInt32();
        row.randomSuffix = fields[119].GetUInt32();
        row.itemSet = fields[120].GetUInt32();
        row.maxDurability = fields[121].GetUInt32();
        row.area = fields[122].GetUInt32();
        row.map = fields[123].GetUInt32();
        row.bagFamily = fields[124].GetUInt32();
        row.totemCategory = fields[125].GetUInt32();
        row.sockets[0].color = fields[126].GetUInt32();
        row.sockets[0].content = fields[127].GetUInt32();
        row.sockets[1].color = fields[128].GetUInt32();
        row.sockets[1].content = fields[129].GetUInt32();
        row.sockets[2].color = fields[130].GetUInt32();
        row.sockets[2].content = fields[131].GetUInt32();
        row.socketBonus = fields[132].GetUInt32();
        row.gemProperties = fields[133].GetUInt32();
        row.requiredDisenchantSkill = fields[134].GetUInt32();
        row.armorDamageModifier = fields[135].GetFloat();
        row.duration = fields[136].GetUInt32();
        row.itemLimitCategory = fields[137].GetUInt32();
        row.holidayId = fields[138].GetUInt32();
        row.disenchantId = fields[139].GetUInt32();
        row.foodType = fields[140].GetUInt32();
        row.statScalingFactor = fields[141].GetFloat();
        row.unknown400_1 = fields[142].GetUInt32();
        row.unknown400_2 = fields[143].GetUInt32();
        row.minMoneyLoot = fields[144].GetUInt32();
        row.maxMoneyLoot = fields[145].GetUInt32();
        row.extraFlags = fields[146].GetUInt32();
        return row;
    }

    TableRead<ItemTemplateRow> QueryItemTemplates()
    {
        // the columns in table order, 0 to 146
        return ReadWorldTable<ItemTemplateRow>("SELECT `entry`, `class`, `subclass`, `unk0`, `name`, `displayid`, `Quality`, `Flags`, `Flags2`, `Unknown`, `Unknown1`, `Unknown2`, `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`, `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`, `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `stat_type1`, `stat_value1`, `stat_unk1_1`, `stat_unk2_1`, `stat_type2`, `stat_value2`, `stat_unk1_2`, `stat_unk2_2`, `stat_type3`, `stat_value3`, `stat_unk1_3`, `stat_unk2_3`, `stat_type4`, `stat_value4`, `stat_unk1_4`, `stat_unk2_4`, `stat_type5`, `stat_value5`, `stat_unk1_5`, `stat_unk2_5`, `stat_type6`, `stat_value6`, `stat_unk1_6`, `stat_unk2_6`, `stat_type7`, `stat_value7`, `stat_unk1_7`, `stat_unk2_7`, `stat_type8`, `stat_value8`, `stat_unk1_8`, `stat_unk2_8`, `stat_type9`, `stat_value9`, `stat_unk1_9`, `stat_unk2_9`, `stat_type10`, `stat_value10`, `stat_unk1_10`, `stat_unk2_10`, `ScalingStatDistribution`, `DamageType`, `delay`, `RangedModRange`, `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`, `spellid_2`, `spelltrigger_2`, `spellcharges_2`, `spellppmRate_2`, `spellcooldown_2`, `spellcategory_2`, `spellcategorycooldown_2`, `spellid_3`, `spelltrigger_3`, `spellcharges_3`, `spellppmRate_3`, `spellcooldown_3`, `spellcategory_3`, `spellcategorycooldown_3`, `spellid_4`, `spelltrigger_4`, `spellcharges_4`, `spellppmRate_4`, `spellcooldown_4`, `spellcategory_4`, `spellcategorycooldown_4`, `spellid_5`, `spelltrigger_5`, `spellcharges_5`, `spellppmRate_5`, `spellcooldown_5`, `spellcategory_5`, `spellcategorycooldown_5`, `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`, `RandomProperty`, `RandomSuffix`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`, `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`, `RequiredDisenchantSkill`, `ArmorDamageModifier`, `Duration`, `ItemLimitCategory`, `HolidayId`, `DisenchantID`, `FoodType`, `StatScalingFactor`, `Unknown400_1`, `Unknown400_2`, `minMoneyLoot`, `maxMoneyLoot`, `ExtraFlags` FROM `item_template`", ReadRow);
    }
}
