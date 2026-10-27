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

#ifndef MANGOS_H_ITEMTEMPLATEREPOSITORY
#define MANGOS_H_ITEMTEMPLATEREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

#include <string>

class Field;

/// The columns of an `item_template` row, and how many of each repeated group it holds.
enum
{
    ITEM_TEMPLATE_COLUMNS = 147,
    ITEM_TEMPLATE_STAT_COLUMNS = 10,
    ITEM_TEMPLATE_SPELL_COLUMNS = 5,
    ITEM_TEMPLATE_SOCKET_COLUMNS = 3
};

/// One of the stat columns `stat_type1` to `stat_unk2_10`, one per stat.
struct ItemTemplateStatRow
{
    uint32 type = 0;
    uint32 value = 0;
    uint32 unk1 = 0;
    uint32 unk2 = 0;
};

/// One of the spell columns `spellid_1` to `spellcategorycooldown_5`, one per spell.
struct ItemTemplateSpellRow
{
    uint32 id = 0;
    uint32 trigger = 0;
    uint32 charges = 0;
    float ppmRate = 0.0f;
    uint32 cooldown = 0;
    uint32 category = 0;
    uint32 categoryCooldown = 0;
};

/// One of the socket columns `socketColor_1` to `socketContent_3`, one per socket.
struct ItemTemplateSocketRow
{
    uint32 color = 0;
    uint32 content = 0;
};

/// One `item_template` row, a member per column in table order, the stats, spells and sockets as arrays.
struct ItemTemplateRow
{
    uint32 entry = 0;
    uint32 itemClass = 0;
    uint32 subClass = 0;
    uint32 unk0 = 0;
    std::string name;
    uint32 displayId = 0;
    uint32 quality = 0;
    uint32 flags = 0;
    uint32 flags2 = 0;
    float unknown = 0.0f;
    float unknown1 = 0.0f;
    uint32 unknown2 = 0;
    uint32 buyCount = 0;
    uint32 buyPrice = 0;
    uint32 sellPrice = 0;
    uint32 inventoryType = 0;
    uint32 allowableClass = 0;
    uint32 allowableRace = 0;
    uint32 itemLevel = 0;
    uint32 requiredLevel = 0;
    uint32 requiredSkill = 0;
    uint32 requiredSkillRank = 0;
    uint32 requiredSpell = 0;
    uint32 requiredHonorRank = 0;
    uint32 requiredCityRank = 0;
    uint32 requiredReputationFaction = 0;
    uint32 requiredReputationRank = 0;
    uint32 maxCount = 0;
    uint32 stackable = 0;
    uint32 containerSlots = 0;
    ItemTemplateStatRow stats[ITEM_TEMPLATE_STAT_COLUMNS];
    uint32 scalingStatDistribution = 0;
    uint32 damageType = 0;
    uint32 delay = 0;
    float rangedModRange = 0.0f;
    ItemTemplateSpellRow spells[ITEM_TEMPLATE_SPELL_COLUMNS];
    uint32 bonding = 0;
    std::string description;
    uint32 pageText = 0;
    uint32 languageId = 0;
    uint32 pageMaterial = 0;
    uint32 startQuest = 0;
    uint32 lockId = 0;
    uint32 material = 0;
    uint32 sheath = 0;
    uint32 randomProperty = 0;
    uint32 randomSuffix = 0;
    uint32 itemSet = 0;
    uint32 maxDurability = 0;
    uint32 area = 0;
    uint32 map = 0;
    uint32 bagFamily = 0;
    uint32 totemCategory = 0;
    ItemTemplateSocketRow sockets[ITEM_TEMPLATE_SOCKET_COLUMNS];
    uint32 socketBonus = 0;
    uint32 gemProperties = 0;
    uint32 requiredDisenchantSkill = 0;
    float armorDamageModifier = 0.0f;
    uint32 duration = 0;
    uint32 itemLimitCategory = 0;
    uint32 holidayId = 0;
    uint32 disenchantId = 0;
    uint32 foodType = 0;
    float statScalingFactor = 0.0f;
    uint32 unknown400_1 = 0;
    uint32 unknown400_2 = 0;
    uint32 minMoneyLoot = 0;
    uint32 maxMoneyLoot = 0;
    uint32 extraFlags = 0;
};

/// The item templates on the world database.
namespace ItemTemplateRepository
{
    /// Reads one row in the column order of the statement; a NULL text reads as "".
    ItemTemplateRow ReadRow(Field* fields);

    /// Reads every row, synchronously, in the order the database returns them, with how the read ended.
    TableRead<ItemTemplateRow> QueryItemTemplates();
}

#endif
