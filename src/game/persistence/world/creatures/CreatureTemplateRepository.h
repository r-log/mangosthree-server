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

#ifndef MANGOS_H_CREATURETEMPLATEREPOSITORY
#define MANGOS_H_CREATURETEMPLATEREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

#include <string>

class Field;

/// The repeated columns of a `creature_template` row.
enum
{
    CREATURE_TEMPLATE_DIFFICULTY_COLUMNS = 3,
    CREATURE_TEMPLATE_MODEL_COLUMNS = 4,
    CREATURE_TEMPLATE_KILL_CREDIT_COLUMNS = 2,
    CREATURE_TEMPLATE_QUEST_ITEM_COLUMNS = 6
};

/// One `creature_template` row, a member per column in table order, the repeated columns as arrays.
struct CreatureTemplateRow
{
    uint32 entry = 0;
    std::string name;
    std::string subName;
    std::string iconName;
    uint32 minLevel = 0;
    uint32 maxLevel = 0;
    uint32 difficultyEntry[CREATURE_TEMPLATE_DIFFICULTY_COLUMNS] = {};
    uint32 modelId[CREATURE_TEMPLATE_MODEL_COLUMNS] = {};
    uint32 factionAlliance = 0;
    uint32 factionHorde = 0;
    float scale = 0.0f;
    uint32 family = 0;
    uint32 creatureType = 0;
    uint32 inhabitType = 0;
    uint32 regenerateStats = 0;
    uint32 racialLeader = 0;
    uint32 npcFlags = 0;
    uint32 unitFlags = 0;
    uint32 unitFlags2 = 0;
    uint32 dynamicFlags = 0;
    uint32 extraFlags = 0;
    uint32 creatureTypeFlags = 0;
    float speedWalk = 0.0f;
    float speedRun = 0.0f;
    uint32 unitClass = 0;
    uint32 rank = 0;
    uint32 expansion = 0;
    float healthMultiplier = 0.0f;
    float powerMultiplier = 0.0f;
    float damageMultiplier = 0.0f;
    float damageVariance = 0.0f;
    float armorMultiplier = 0.0f;
    float experienceMultiplier = 0.0f;
    uint32 minLevelHealth = 0;
    uint32 maxLevelHealth = 0;
    uint32 minLevelMana = 0;
    uint32 maxLevelMana = 0;
    float minMeleeDmg = 0.0f;
    float maxMeleeDmg = 0.0f;
    float minRangedDmg = 0.0f;
    float maxRangedDmg = 0.0f;
    uint32 armor = 0;
    uint32 meleeAttackPower = 0;
    uint32 rangedAttackPower = 0;
    uint32 meleeBaseAttackTime = 0;
    uint32 rangedBaseAttackTime = 0;
    uint32 damageSchool = 0;
    uint32 minLootGold = 0;
    uint32 maxLootGold = 0;
    uint32 lootId = 0;
    uint32 pickpocketLootId = 0;
    uint32 skinningLootId = 0;
    uint32 killCredit[CREATURE_TEMPLATE_KILL_CREDIT_COLUMNS] = {};
    uint32 questItems[CREATURE_TEMPLATE_QUEST_ITEM_COLUMNS] = {};
    uint32 mechanicImmuneMask = 0;
    uint32 resistanceHoly = 0;
    uint32 resistanceFire = 0;
    uint32 resistanceNature = 0;
    uint32 resistanceFrost = 0;
    uint32 resistanceShadow = 0;
    uint32 resistanceArcane = 0;
    uint32 petSpellDataId = 0;
    uint32 movementType = 0;
    uint32 movementTemplateId = 0;
    uint32 trainerType = 0;
    uint32 trainerSpell = 0;
    uint32 trainerClass = 0;
    uint32 trainerRace = 0;
    uint32 trainerTemplateId = 0;
    uint32 vendorTemplateId = 0;
    uint32 equipmentTemplateId = 0;
    uint32 vehicleTemplateId = 0;
    uint32 gossipMenuId = 0;
    std::string aiName;
};

/// The creature templates on the world database.
namespace CreatureTemplateRepository
{
    /// Reads one row in the column order of the statement; a NULL text reads as "".
    CreatureTemplateRow ReadRow(Field* fields);

    /// Reads every row, synchronously, in the order the database returns them, with how the read ended.
    TableRead<CreatureTemplateRow> QueryCreatureTemplates();
}

#endif
