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

/**
 * @file ObjectMgrCreatures.cpp
 * @brief ObjectMgr's creature template, addon, class-level stat, equipment, model and spawn loaders and the creature
 * grid placement.
 */

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "Utilities/PackedValues.h"
#include "ObjectMgr.h"
#include "LivingWorldAnchorPolicy.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "Log.h"
#include "ObjectGuid.h"
#include "data/ScriptNameStore.h"
#include "spells/data/SpellMgr.h"
#include "data/GameConfig.h"
#include "data/MapGeometry.h"
#include "ProgressBar.h"
#include "data/Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Util.h"
#include "WaypointManager.h"
#include "DB2Structure.h"
#include "DB2Stores.h"
#include "DisableMgr.h"

#include <limits>
#include "persistence/world/creatures/CreatureDataRepository.h"
#include "persistence/world/creatures/CreatureAddonRepository.h"
#include "persistence/world/creatures/CreatureEquipmentRepository.h"
#include "persistence/world/creatures/CreatureModelInfoRepository.h"
#include "persistence/world/creatures/CreatureTemplateRepository.h"
#include "persistence/world/creatures/VehicleAccessoryRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

static_assert(int(CREATURE_TEMPLATE_DIFFICULTY_COLUMNS) == int(MAX_DIFFICULTY - 1), "a creature template row holds every difficulty entry");
static_assert(int(CREATURE_TEMPLATE_MODEL_COLUMNS) == int(MAX_CREATURE_MODEL), "a creature template row holds every model");
static_assert(int(CREATURE_TEMPLATE_KILL_CREDIT_COLUMNS) == int(MAX_KILL_CREDIT), "a creature template row holds every kill credit");
static_assert(sizeof(CreatureInfo().QuestItems) == CREATURE_TEMPLATE_QUEST_ITEM_COLUMNS * sizeof(uint32), "a creature template row holds every quest item");

/**
 * @brief Loads creature template definitions and validates related data.
 */
void ObjectMgr::LoadCreatureTemplates()
{
    TableRead<CreatureTemplateRow> read = CreatureTemplateRepository::QueryCreatureTemplates();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `creature_template` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("creature_template table is empty!\n");
    }
    else
    {
        m_creatureTemplates.Clear();
        m_creatureTemplateTexts.Clear();
        BarGoLink bar(read.rows.size());
        for (CreatureTemplateRow const& row : read.rows)
        {
            bar.step();
            CreatureInfo info = CreatureInfo();
            info.Entry = row.entry;
            info.MinLevel = row.minLevel;
            info.MaxLevel = row.maxLevel;
            for (uint32 k = 0; k < CREATURE_TEMPLATE_DIFFICULTY_COLUMNS; ++k)
            {
                info.DifficultyEntry[k] = row.difficultyEntry[k];
            }
            for (uint32 k = 0; k < CREATURE_TEMPLATE_MODEL_COLUMNS; ++k)
            {
                info.ModelId[k] = row.modelId[k];
            }
            info.FactionAlliance = row.factionAlliance;
            info.FactionHorde = row.factionHorde;
            info.Scale = row.scale;
            info.Family = row.family;
            info.CreatureType = row.creatureType;
            info.InhabitType = row.inhabitType;
            info.RegenerateStats = row.regenerateStats;
            info.RacialLeader = int32(row.racialLeader);
            info.NpcFlags = row.npcFlags;
            info.UnitFlags = row.unitFlags;
            info.UnitFlags2 = row.unitFlags2;
            info.DynamicFlags = row.dynamicFlags;
            info.ExtraFlags = row.extraFlags;
            info.CreatureTypeFlags = row.creatureTypeFlags;
            info.SpeedWalk = row.speedWalk;
            info.SpeedRun = row.speedRun;
            info.UnitClass = row.unitClass;
            info.Rank = row.rank;
            info.Expansion = int32(row.expansion);
            info.HealthMultiplier = row.healthMultiplier;
            info.PowerMultiplier = row.powerMultiplier;
            info.DamageMultiplier = row.damageMultiplier;
            info.DamageVariance = row.damageVariance;
            info.ArmorMultiplier = row.armorMultiplier;
            info.ExperienceMultiplier = row.experienceMultiplier;
            info.MinLevelHealth = row.minLevelHealth;
            info.MaxLevelHealth = row.maxLevelHealth;
            info.MinLevelMana = row.minLevelMana;
            info.MaxLevelMana = row.maxLevelMana;
            info.MinMeleeDmg = row.minMeleeDmg;
            info.MaxMeleeDmg = row.maxMeleeDmg;
            info.MinRangedDmg = row.minRangedDmg;
            info.MaxRangedDmg = row.maxRangedDmg;
            info.Armor = row.armor;
            info.MeleeAttackPower = row.meleeAttackPower;
            info.RangedAttackPower = row.rangedAttackPower;
            info.MeleeBaseAttackTime = row.meleeBaseAttackTime;
            info.RangedBaseAttackTime = row.rangedBaseAttackTime;
            info.DamageSchool = row.damageSchool;
            info.MinLootGold = row.minLootGold;
            info.MaxLootGold = row.maxLootGold;
            info.LootId = row.lootId;
            info.PickpocketLootId = row.pickpocketLootId;
            info.SkinningLootId = row.skinningLootId;
            for (uint32 k = 0; k < CREATURE_TEMPLATE_KILL_CREDIT_COLUMNS; ++k)
            {
                info.KillCredit[k] = row.killCredit[k];
            }
            for (uint32 k = 0; k < CREATURE_TEMPLATE_QUEST_ITEM_COLUMNS; ++k)
            {
                info.QuestItems[k] = row.questItems[k];
            }
            info.MechanicImmuneMask = row.mechanicImmuneMask;
            info.ResistanceHoly = int32(row.resistanceHoly);
            info.ResistanceFire = int32(row.resistanceFire);
            info.ResistanceNature = int32(row.resistanceNature);
            info.ResistanceFrost = int32(row.resistanceFrost);
            info.ResistanceShadow = int32(row.resistanceShadow);
            info.ResistanceArcane = int32(row.resistanceArcane);
            info.PetSpellDataId = row.petSpellDataId;
            info.MovementType = row.movementType;
            info.MovementTemplateId = row.movementTemplateId;
            info.TrainerType = row.trainerType;
            info.TrainerSpell = row.trainerSpell;
            info.TrainerClass = row.trainerClass;
            info.TrainerRace = row.trainerRace;
            info.TrainerTemplateId = row.trainerTemplateId;
            info.VendorTemplateId = row.vendorTemplateId;
            info.EquipmentTemplateId = row.equipmentTemplateId;
            info.VehicleTemplateId = row.vehicleTemplateId;
            info.GossipMenuId = row.gossipMenuId;
            info.Name = m_creatureTemplateTexts.Add(row.name);
            info.SubName = m_creatureTemplateTexts.Add(row.subName);
            info.IconName = m_creatureTemplateTexts.Add(row.iconName);
            info.AIName = m_creatureTemplateTexts.Add(row.aiName);
            m_creatureTemplates.Set(row.entry, info);
        }
        loaded = uint32(read.rows.size());
    }

    sLog.outString(">> Loaded %u creature definitions", loaded);
    sLog.outString();

    std::set<uint32> difficultyEntries[MAX_DIFFICULTY - 1]; // already loaded difficulty 1 value in creatures
    std::set<uint32> hasDifficultyEntries[MAX_DIFFICULTY - 1]; // already loaded creatures with difficulty 1  values

    // check data correctness
    for (uint32 i = 1; i < m_creatureTemplates.GetMaxEntry(); ++i)
    {
        CreatureInfo* cInfo = m_creatureTemplates.FindForEdit(i);
        if (!cInfo)
        {
            continue;
        }

        bool ok = true;                                     // bool to allow continue outside this loop
        for (uint32 diff = 0; diff < MAX_DIFFICULTY - 1 && ok; ++diff)
        {
            if (!cInfo->DifficultyEntry[diff])
            {
                continue;
            }
            ok = false;                                     // will be set to true at the end of this loop again

            CreatureInfo const* difficultyInfo = GetCreatureTemplate(cInfo->DifficultyEntry[diff]);
            if (!difficultyInfo)
            {
                sLog.outErrorDb("Creature (Entry: %u) have `difficulty_entry_%u`=%u but creature entry %u not exist.",
                                i, diff + 1, cInfo->DifficultyEntry[diff], cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (difficultyEntries[diff].find(i) != difficultyEntries[diff].end())
            {
                sLog.outErrorDb("Creature (Entry: %u) listed as difficulty %u but have value in `difficulty_entry_%u`.", i, diff + 1, diff + 1);
                continue;
            }

            bool ok2 = true;
            for (uint32 diff2 = 0; diff2 < MAX_DIFFICULTY - 1 && ok2; ++diff2)
            {
                ok2 = false;
                if (difficultyEntries[diff2].find(cInfo->DifficultyEntry[diff]) != difficultyEntries[diff2].end())
                {
                    sLog.outErrorDb("Creature (Entry: %u) already listed as difficulty %u for another entry.", cInfo->DifficultyEntry[diff], diff2 + 1);
                    continue;
                }

                if (hasDifficultyEntries[diff2].find(cInfo->DifficultyEntry[diff]) != hasDifficultyEntries[diff2].end())
                {
                    sLog.outErrorDb("Creature (Entry: %u) have `difficulty_entry_%u`=%u but creature entry %u have difficulty %u entry also.",
                                    i, diff + 1, cInfo->DifficultyEntry[diff], cInfo->DifficultyEntry[diff], diff2 + 1);
                    continue;
                }
                ok2 = true;
            }
            if (!ok2)
            {
                continue;
            }

            if (cInfo->UnitClass != difficultyInfo->UnitClass)
            {
                sLog.outErrorDb("Creature (Entry: %u, class %u) has different `UnitClass` in difficulty %u mode (Entry: %u, class %u).",
                                i, cInfo->UnitClass, diff + 1, cInfo->DifficultyEntry[diff], difficultyInfo->UnitClass);
                continue;
            }

            if (cInfo->NpcFlags != difficultyInfo->NpcFlags)
            {
                sLog.outErrorDb("Creature (Entry: %u) has different `NpcFlags` in difficulty %u mode (Entry: %u).", i, diff + 1, cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (cInfo->TrainerClass != difficultyInfo->TrainerClass)
            {
                sLog.outErrorDb("Creature (Entry: %u) has different `TrainerClass` in difficulty %u mode (Entry: %u).", i, diff + 1, cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (cInfo->TrainerRace != difficultyInfo->TrainerRace)
            {
                sLog.outErrorDb("Creature (Entry: %u) has different `TrainerRace` in difficulty %u mode (Entry: %u).", i, diff + 1, cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (cInfo->TrainerType != difficultyInfo->TrainerType)
            {
                sLog.outErrorDb("Creature (Entry: %u) has different `TrainerType` in difficulty %u mode (Entry: %u).", i, diff + 1, cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (cInfo->TrainerSpell != difficultyInfo->TrainerSpell)
            {
                sLog.outErrorDb("Creature (Entry: %u) has different `TrainerSpell` in difficulty %u mode (Entry: %u).", i, diff + 1, cInfo->DifficultyEntry[diff]);
                continue;
            }

            if (difficultyInfo->AIName && *difficultyInfo->AIName)
            {
                sLog.outErrorDb("Difficulty %u mode creature (Entry: %u) has `AIName`, but in any case will used difficulty 0 mode creature (Entry: %u) AIName.",
                                diff + 1, cInfo->DifficultyEntry[diff], i);
                continue;
            }

            if (sScriptNameStore.GetBoundScriptId(SCRIPTED_UNIT, difficultyInfo->Entry))
            {
                sLog.outErrorDb("Difficulty %u mode creature (Entry: %u) has `ScriptName`, but in any case will used difficulty 0 mode creature (Entry: %u) ScriptName.",
                                diff + 1, cInfo->DifficultyEntry[diff], i);
                continue;
            }

            hasDifficultyEntries[diff].insert(i);
            difficultyEntries[diff].insert(cInfo->DifficultyEntry[diff]);
            ok = true;
        }
        if (!ok)
        {
            continue;
        }

        FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->FactionAlliance);
        if (!factionTemplate)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent faction_A template (%u)", cInfo->Entry, cInfo->FactionAlliance);
        }

        factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->FactionHorde);
        if (!factionTemplate)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent faction_H template (%u)", cInfo->Entry, cInfo->FactionHorde);
        }

        for (int k = 0; k < MAX_KILL_CREDIT; ++k)
        {
            if (cInfo->KillCredit[k])
            {
                if (!GetCreatureTemplate(cInfo->KillCredit[k]))
                {
                    sLog.outErrorDb("Creature (Entry: %u) has nonexistent creature entry in `KillCredit%d` (%u)", cInfo->Entry, k + 1, cInfo->KillCredit[k]);
                    cInfo->KillCredit[k] = 0;
                }
            }
        }

        // used later for scale
        CreatureDisplayInfoEntry const* displayScaleEntry = NULL;

        for (int j = 0; j < MAX_CREATURE_MODEL; ++j)
        {
            if (cInfo->ModelId[j])
            {
                CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(cInfo->ModelId[j]);
                if (!displayEntry)
                {
                    sLog.outErrorDb("Creature (Entry: %u) has nonexistent modelid_%d (%u), can crash client", cInfo->Entry, j + 1, cInfo->ModelId[j]);
                    cInfo->ModelId[j] = 0;
                }
                else if (!displayScaleEntry)
                {
                    displayScaleEntry = displayEntry;
                }

                CreatureModelInfo const* minfo = GetCreatureModelInfo(cInfo->ModelId[j]);
                if (!minfo)
                {
                    sLog.outErrorDb("Creature (Entry: %u) are using modelid_%d (%u), but creature_model_info are missing for this model.", cInfo->Entry, j + 1, cInfo->ModelId[j]);
                }
            }
        }

        if (!displayScaleEntry)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent modelid in modelid_1/modelid_2/modelid_3/modelid_4", cInfo->Entry);
        }

        if (!cInfo->MinLevel)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid minlevel, set to 1", cInfo->Entry);
            cInfo->MinLevel = 1;
        }

        if (cInfo->MinLevel > cInfo->MaxLevel)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid maxlevel, set to minlevel", cInfo->Entry);
            cInfo->MaxLevel = cInfo->MinLevel;
        }

        if (cInfo->MinLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Creature (Entry: %u) `MinLevel` exceeds maximum allowed value of '%u'", cInfo->Entry, uint32(DEFAULT_MAX_CREATURE_LEVEL));
            cInfo->MinLevel = uint32(DEFAULT_MAX_CREATURE_LEVEL);
        }

        if (cInfo->MaxLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Creature (Entry: %u) `MaxLevel` exceeds maximum allowed value of '%u'", cInfo->Entry, uint32(DEFAULT_MAX_CREATURE_LEVEL));
            cInfo->MaxLevel = uint32(DEFAULT_MAX_CREATURE_LEVEL);
        }

        if (cInfo->Expansion > MAX_EXPANSION)
        {
            sLog.outErrorDb("Creature (Entry: %u) `Expansion(%u)` is not correct", cInfo->Entry, uint32(MAX_EXPANSION));
            cInfo->Expansion = -1;
        }

        // use below code for 0-checks for unit_class
        if (!cInfo->UnitClass || (((1 << (cInfo->UnitClass - 1)) & CLASSMASK_ALL_CREATURES) == 0))
        {
            sLog.outErrorDb("Creature (Entry: %u) does not have proper `UnitClass(%u)` in creature_template", cInfo->Entry, cInfo->UnitClass);
           // Mark NPC as having improper data by his expansion
            cInfo->Expansion = -1;
        }

        if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK) && cInfo->Expansion >= 0) // TODO - Remove the DB_STRICTED_CHECK after a while
        {
            // check if ClassLevel data are available for all possible level of that creature
            for (uint32 level = cInfo->MinLevel; level <= cInfo->MaxLevel; ++level)
            {
                if (!GetCreatureClassLvlStats(level, cInfo->UnitClass, cInfo->Expansion))
                {
                    sLog.outErrorDb("Creature (Entry: %u), Class(%u), level(%u) has no data in `creature_template_classlevelstats`", cInfo->Entry, cInfo->UnitClass, level);
                         // Deactivate using ClassLevelStats for this NPC
                    cInfo->Expansion = -1;
                    break;
                }
            }
        }

        if (cInfo->DamageSchool >= MAX_SPELL_SCHOOL)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid spell school value (%u) in `dmgschool`", cInfo->Entry, cInfo->DamageSchool);
            cInfo->DamageSchool = SPELL_SCHOOL_NORMAL;
        }

        if (cInfo->MeleeAttackPower == 0)
        {
            cInfo->MeleeAttackPower  = BASE_ATTACK_TIME;
        }

        if (cInfo->RangedAttackPower == 0)
        {
            cInfo->RangedAttackPower = BASE_ATTACK_TIME;
        }

        if (cInfo->NpcFlags & UNIT_NPC_FLAG_SPELLCLICK)
        {
            sLog.outDebug("Creature (Entry: %u) has dynamic flag UNIT_NPC_FLAG_SPELLCLICK (%u) set, but it is expected to be set in run-time based at `npc_spellclick_spells` contents.", cInfo->Entry, UNIT_NPC_FLAG_SPELLCLICK);
            cInfo->NpcFlags &= ~UNIT_NPC_FLAG_SPELLCLICK;
        }

        if ((cInfo->NpcFlags & UNIT_NPC_FLAG_TRAINER) && cInfo->TrainerType >= MAX_TRAINER_TYPE)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong trainer type %u", cInfo->Entry, cInfo->TrainerType);
        }

        if (cInfo->CreatureType && !sCreatureTypeStore.LookupEntry(cInfo->CreatureType))
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid creature type (%u) in `type`", cInfo->Entry, cInfo->CreatureType);
            cInfo->CreatureType = CREATURE_TYPE_HUMANOID;
        }

        // must exist or used hidden but used in data horse case
        if (cInfo->Family && !sCreatureFamilyStore.LookupEntry(cInfo->Family) && cInfo->Family != CREATURE_FAMILY_HORSE_CUSTOM)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid creature family (%u) in `family`", cInfo->Entry, cInfo->Family);
            cInfo->Family = 0;
        }

        if (cInfo->InhabitType <= 0 || cInfo->InhabitType > INHABIT_ANYWHERE)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong value (%u) in `InhabitType`, creature will not correctly walk/swim/fly", cInfo->Entry, cInfo->InhabitType);
            cInfo->InhabitType = INHABIT_ANYWHERE;
        }

        if (cInfo->PetSpellDataId)
        {
            CreatureSpellDataEntry const* spellDataId = sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId);
            if (!spellDataId)
            {
                sLog.outErrorDb("Creature (Entry: %u) has non-existing PetSpellDataId (%u)", cInfo->Entry, cInfo->PetSpellDataId);
            }
        }

        if (cInfo->MovementType >= CREATURE_MOVEMENT_MAX)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong movement generator type (%u), ignore and set to IDLE.", cInfo->Entry, cInfo->MovementType);
            cInfo->MovementType = CREATURE_MOVEMENT_IDLE;
        }

        if (cInfo->VehicleTemplateId && !sVehicleStore.LookupEntry(cInfo->VehicleTemplateId))
        {
            sLog.outErrorDb("Creature (Entry: %u) has non-existing vehicle_id (%u), set to 0.", cInfo->Entry, cInfo->VehicleTemplateId);
            cInfo->VehicleTemplateId = 0;
        }

        if (cInfo->EquipmentTemplateId > 0)                         // 0 no equipment
        {
            if (!GetEquipmentInfo(cInfo->EquipmentTemplateId))
            {
                sLog.outErrorDb("Table `creature_template` have creature (Entry: %u) with EquipmentTemplateId %u not found in table `creature_equip_template`, set to no equipment.", cInfo->Entry, cInfo->EquipmentTemplateId);
                cInfo->EquipmentTemplateId = 0;
            }
        }

        if (cInfo->VendorTemplateId > 0)
        {
            if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
            {
                sLog.outErrorDb("Table `creature_template` have creature (Entry: %u) with VendorTemplateId %u but not have flag UNIT_NPC_FLAG_VENDOR (%u), vendor items will ignored.", cInfo->Entry, cInfo->VendorTemplateId, UNIT_NPC_FLAG_VENDOR);
            }
        }

        /// if not set custom creature scale then load scale from CreatureDisplayInfo.dbc
        if (cInfo->Scale <= 0.0f)
        {
            if (displayScaleEntry)
            {
                cInfo->Scale = displayScaleEntry->CreatureModelScale;
            }
            else
            {
                cInfo->Scale = DEFAULT_OBJECT_SCALE;
            }
        }
    }

    sLog.outString(">> Loaded %u creature definitions", loaded);
    sLog.outString();
}

/**
 * @brief Converts serialized creature addon aura text into stored spell ids.
 *
 * @param text The aura text, "spell1 spell2 ...".
 * @param guidOrEntry The addon's key, used in log output.
 * @param table The source table name.
 * @param guidEntryStr The identifier label used in log output.
 * @return The accepted spell ids then a 0, or empty when the text names no spell.
 */
static std::vector<uint32> ConvertCreatureAddonAuras(std::string const& text, uint32 guidOrEntry, char const* table, char const* guidEntryStr)
{
    // Now add the auras, format "spell1 spell2 ..."
    char const* p, *s;
    std::vector<int> val;
    s = p = text.c_str();
    while (p[0] != 0)
    {
        ++p;
        if (p[0] == ' ')
        {
            val.push_back(atoi(s));
            s = ++p;
        }
    }
    if (p != s)
    {
        val.push_back(atoi(s));
    }

    std::vector<uint32> auras;

    // empty list
    if (val.empty())
    {
        return auras;
    }

    for (uint32 j = 0; j < val.size(); ++j)
    {
        uint32 cAura = uint32(val[j]);

        SpellEntry const* AdditionalSpellInfo = sSpellStore.LookupEntry(cAura);
        if (!AdditionalSpellInfo)
        {
            sLog.outErrorDb("Creature (%s: %u) has wrong spell %u defined in `auras` field in `%s`.", guidEntryStr, guidOrEntry, cAura, table);
            continue;
        }

        // Must be Aura, but also allow dummy/script effect spells, as they are used sometimes to select a random aura or similar
        if (!IsSpellAppliesAura(AdditionalSpellInfo) && !IsSpellHaveEffect(AdditionalSpellInfo, SPELL_EFFECT_DUMMY) && !IsSpellHaveEffect(AdditionalSpellInfo, SPELL_EFFECT_SCRIPT_EFFECT) && !IsSpellHaveEffect(AdditionalSpellInfo, SPELL_EFFECT_TRIGGER_SPELL))
        {
            sLog.outErrorDb("Creature (%s: %u) has spell %u defined in `auras` field in `%s, but spell doesn't apply an aura`.", guidEntryStr, guidOrEntry, cAura, table);
            continue;
        }

        // TODO: Remove LogFilter check after more research
        if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK) && !IsOnlySelfTargeting(AdditionalSpellInfo))
        {
            sLog.outErrorDb("Creature (%s: %u) has spell %u defined in `auras` field in `%s, but spell is no self-only spell`.", guidEntryStr, guidOrEntry, cAura, table);
            continue;
        }

        if (IsSpellHaveAura(AdditionalSpellInfo, SPELL_AURA_CONTROL_VEHICLE))
        {
            sLog.outErrorDb("Creature (%s: %u) has spell %u defined in `auras` field in `%s, but vehicle control spells are not suitable for _addon.auras handling`.", guidEntryStr, guidOrEntry, cAura, table);
            continue;
        }

        if (std::find(auras.begin(), auras.end(), cAura) != auras.end())
        {
            sLog.outErrorDb("Creature (%s: %u) has duplicate spell %u defined in `auras` field in `%s`.", guidEntryStr, guidOrEntry, cAura, table);
            continue;
        }

        auras.push_back(cAura);
    }

    // fill terminator element (after last added)
    auras.push_back(0);
    return auras;
}

/**
 * @brief Loads creature addon records of one table into a store and validates their fields.
 *
 * @param addons The store to fill.
 * @param read The table's read.
 * @param table The source table name.
 * @param entryName The entry label used in log output.
 * @param comment The descriptive text used in the load summary.
 */
static void LoadCreatureAddonTable(CreatureAddonStore& addons, TableRead<CreatureAddonRow> const& read, char const* table, char const* entryName, char const* comment)
{
    if (read.Unreadable())
    {
        sLog.outError("Error loading `%s` table: %s\n", table, TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    std::map<uint32, std::string> auraTexts;
    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("%s table is empty!\n", table);
    }
    else
    {
        addons.Clear();
        BarGoLink bar(read.rows.size());
        for (CreatureAddonRow const& row : read.rows)
        {
            bar.step();
            CreatureDataAddon addon;
            addon.guidOrEntry = row.guidOrEntry;
            addon.mount = row.mount;
            addon.bytes1 = row.bytes1;
            addon.sheath_state = row.sheathState;
            addon.pvp_state = row.pvpState;
            addon.emote = row.emote;
            addon.splineFlags = row.moveFlags;
            addon.auras = NULL;
            addons.Set(addon);
            auraTexts[row.guidOrEntry] = row.auras;
        }
        loaded = uint32(read.rows.size());
    }

    // check data correctness and convert 'auras'
    for (uint32 i = 1; i < addons.GetMaxEntry(); ++i)
    {
        CreatureDataAddon const* addon = addons.Find(i);
        if (!addon)
        {
            continue;
        }

        if (addon->mount)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(addon->mount))
            {
                sLog.outErrorDb("Creature (%s %u) have invalid displayInfoId for mount (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->mount, table);
                addons.FindForEdit(i)->mount = 0;
            }
        }

        if (addon->sheath_state > SHEATH_STATE_RANGED)
        {
            sLog.outErrorDb("Creature (%s %u) has unknown sheath state (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->sheath_state, table);
        }

        if (!sEmotesStore.LookupEntry(addon->emote))
        {
            sLog.outErrorDb("Creature (%s %u) have invalid emote (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->emote, table);
            addons.FindForEdit(i)->emote = 0;
        }

        addons.SetAuras(i, ConvertCreatureAddonAuras(auraTexts[i], addon->guidOrEntry, table, entryName));
    }

    sLog.outString(">> Loaded %u %s", loaded, comment);
}

/**
 * @brief Loads creature template and spawn addon records.
 */
void ObjectMgr::LoadCreatureAddons()
{
    LoadCreatureAddonTable(m_creatureTemplateAddons, CreatureAddonRepository::QueryCreatureTemplateAddons(), "creature_template_addon", "Entry", "creature template addons");

    // check entry ids
    for (uint32 i = 1; i < m_creatureTemplateAddons.GetMaxEntry(); ++i)
    {
        if (CreatureDataAddon const* addon = m_creatureTemplateAddons.Find(i))
        {
            if (!ObjectMgr::GetCreatureTemplate(addon->guidOrEntry))
            {
                sLog.outErrorDb("Creature (Entry: %u) does not exist but has a record in `%s`", addon->guidOrEntry, "creature_template_addon");
            }
        }
    }

    LoadCreatureAddonTable(m_creatureAddons, CreatureAddonRepository::QueryCreatureAddons(), "creature_addon", "GUID", "creature addons");

    // check entry ids
    for (uint32 i = 1; i < m_creatureAddons.GetMaxEntry(); ++i)
    {
        if (CreatureDataAddon const* addon = m_creatureAddons.Find(i))
        {
            if (mCreatureDataMap.find(addon->guidOrEntry) == mCreatureDataMap.end())
            {
                sLog.outErrorDb("Creature (GUID: %u) does not exist but has a record in `creature_addon`", addon->guidOrEntry);
            }
        }
    }
}

/**
 * @brief Loads creature class and level base stat data.
 */
void ObjectMgr::LoadCreatureClassLvlStats()
{
    // initialize data array
    memset(&m_creatureClassLvlStats, 0, sizeof(m_creatureClassLvlStats));

    std::vector<ClassLevelStatsRow> rows = CreatureDataRepository::QueryClassLevelStats(MAX_EXPANSION);

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb("DB table `creature_template_classlevelstats` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());
    uint32 DataCount = 0;

    for (ClassLevelStatsRow const& row : rows)
    {
        bar.step();

        uint32 creatureClass               = row.classId;
        uint32 creatureLevel               = row.level;

        if (creatureLevel == 0 || creatureLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Found stats for creature level [%u], incorrect level for this core. Skip!", creatureLevel);
            continue;
        }

        if (((1 << (creatureClass - 1)) & CLASSMASK_ALL_CREATURES) == 0)
        {
            sLog.outErrorDb("Found stats for creature class [%u], incorrect class for this core. Skip!", creatureClass);
            continue;
        }

        uint32  baseMana                   = row.baseMana;
        float   baseMeleeAttackPower       = row.baseMeleeAttackPower;
        float   baseRangedAttackPower      = row.baseRangedAttackPower;
        uint32  baseArmor                  = row.baseArmor;

        for (uint8 i = 0; i <= MAX_EXPANSION; ++i)
        {
            CreatureClassLvlStats &cCLS = m_creatureClassLvlStats[creatureLevel][classToIndex[creatureClass]][i - 1];   // values should start from 0
            cCLS.BaseMana                   = baseMana;
            cCLS.BaseMeleeAttackPower       = baseMeleeAttackPower;
            cCLS.BaseRangedAttackPower      = baseRangedAttackPower;
            cCLS.BaseArmor                  = baseArmor;

            cCLS.BaseHealth = row.baseHealth[i];
            cCLS.BaseDamage = row.baseDamage[i];
        }
        ++DataCount;
    }

    sLog.outString(">> Found %u creature stats definitions.", DataCount);
    sLog.outString();
}

/**
 * @brief Gets base class-level stats for a creature level and class.
 *
 * @param level The creature level.
 * @param unitClass The creature unit class.
 * @return The matching stats record, or null if unavailable.
 */
CreatureClassLvlStats const* ObjectMgr::GetCreatureClassLvlStats(uint32 level, uint32 unitClass, int32 expansion) const
{
    if (expansion < 0)
    {
        return NULL;
    }

    CreatureClassLvlStats const* cCLS = &m_creatureClassLvlStats[level][classToIndex[unitClass]][expansion];

    if (cCLS->BaseHealth != 0 && cCLS->BaseDamage > 0.1f)
    {
        return cCLS;
    }

    return NULL;
}

/**
 * @brief Loads creature equipment templates in current and deprecated formats.
 */
void ObjectMgr::LoadEquipmentTemplates()
{
    TableRead<CreatureEquipmentRow> read = CreatureEquipmentRepository::QueryEquipmentTemplates();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `creature_equip_template` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("creature_equip_template table is empty!\n");
    }
    else
    {
        m_creatureEquipment.Clear();
        BarGoLink bar(read.rows.size());
        for (CreatureEquipmentRow const& row : read.rows)
        {
            bar.step();
            EquipmentInfo equipment;
            equipment.entry = row.entry;
            equipment.equipentry[0] = row.equipentry1;
            equipment.equipentry[1] = row.equipentry2;
            equipment.equipentry[2] = row.equipentry3;
            m_creatureEquipment.Set(equipment);
        }
        loaded = uint32(read.rows.size());
    }

    for (uint32 i = 0; i < m_creatureEquipment.GetMaxEntry(); ++i)
    {
        EquipmentInfo const* eqInfo = m_creatureEquipment.Find(i);

        if (!eqInfo)
        {
            continue;
        }

        for (uint8 j = 0; j < 3; ++j)
        {
            if (!eqInfo->equipentry[j])
            {
                continue;
            }

            ItemEntry const* dbcitem = sItemStore.LookupEntry(eqInfo->equipentry[j]);
            if (!dbcitem)
            {
                sLog.outErrorDb("Unknown item (entry=%u) in creature_equip_template.equipentry%u for entry = %u, forced to 0.", eqInfo->equipentry[j], j + 1, i);
                m_creatureEquipment.FindForEdit(i)->equipentry[j] = 0;
                continue;
            }

            if (dbcitem->InventoryType != INVTYPE_WEAPON &&
                    dbcitem->InventoryType != INVTYPE_SHIELD &&
                    dbcitem->InventoryType != INVTYPE_RANGED &&
                    dbcitem->InventoryType != INVTYPE_2HWEAPON &&
                    dbcitem->InventoryType != INVTYPE_WEAPONMAINHAND &&
                    dbcitem->InventoryType != INVTYPE_WEAPONOFFHAND &&
                    dbcitem->InventoryType != INVTYPE_HOLDABLE &&
                    dbcitem->InventoryType != INVTYPE_THROWN &&
                    dbcitem->InventoryType != INVTYPE_RANGEDRIGHT &&
                    dbcitem->InventoryType != INVTYPE_RELIC)
            {
                sLog.outErrorDb("Item (entry=%u) in creature_equip_template.equipentry%u for entry = %u is not equipable in a hand, forced to 0.", eqInfo->equipentry[j], j + 1, i);
                m_creatureEquipment.FindForEdit(i)->equipentry[j] = 0;
            }
        }
    }

    sLog.outString(">> Loaded %u equipment template", loaded);
    sLog.outString();
}

// generally models that does not have a gender(2), or has alternative model for same gender
uint32 ObjectMgr::GetCreatureModelAlternativeModel(uint32 modelId) const
{
    if (const CreatureModelInfo* modelInfo = GetCreatureModelInfo(modelId))
    {
        return modelInfo->modelid_alternative;
    }

    return 0;
}

/**
 * @brief Gets a creature model info record, optionally swapping to the other gender.
 *
 * @param display_id The creature display id.
 * @return The selected model info, or null if none exists.
 */
CreatureModelInfo const* ObjectMgr::GetCreatureModelRandomGender(uint32 display_id) const
{
    CreatureModelInfo const* minfo = GetCreatureModelInfo(display_id);
    if (!minfo)
    {
        return NULL;
    }

    // If a model for another gender exists, 50% chance to use it
    if (minfo->modelid_other_gender != 0 && urand(0, 1) == 0)
    {
        CreatureModelInfo const* minfo_tmp = GetCreatureModelInfo(minfo->modelid_other_gender);
        if (!minfo_tmp)
        {
            sLog.outErrorDb("Model (Entry: %u) has modelid_other_gender %u not found in table `creature_model_info`. ", minfo->modelid, minfo->modelid_other_gender);
            return minfo;                                   // not fatal, just use the previous one
        }
        else
        {
            return minfo_tmp;
        }
    }
    else
    {
        return minfo;
    }
}

uint32 ObjectMgr::GetModelForRace(uint32 sourceModelId, uint32 racemask)
{
    uint32 modelId = 0;

    CreatureModelRaceMapBounds bounds = m_mCreatureModelRaceMap.equal_range(sourceModelId);

    for (CreatureModelRaceMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (!(itr->second.racemask & racemask))
        {
            continue;
        }

        if (itr->second.creature_entry)
        {
            const CreatureInfo* cInfo = GetCreatureTemplate(itr->second.creature_entry);
            modelId = ChooseCreatureDisplayId(cInfo);
        }
        else
        {
            modelId = itr->second.modelid_racial;
        }
    }

    return modelId;
}

/**
 * @brief Loads creature model info records and validates playable race models.
 */
void ObjectMgr::LoadCreatureModelInfo()
{
    TableRead<CreatureModelInfoRow> read = CreatureModelInfoRepository::QueryCreatureModelInfos();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `creature_model_info` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("creature_model_info table is empty!\n");
    }
    else
    {
        m_creatureModels.Clear();
        BarGoLink bar(read.rows.size());
        for (CreatureModelInfoRow const& row : read.rows)
        {
            bar.step();
            CreatureModelInfo model;
            model.modelid = row.modelid;
            model.bounding_radius = row.boundingRadius;
            model.combat_reach = row.combatReach;
            model.gender = row.gender;
            model.modelid_other_gender = row.modelidOtherGender;
            model.modelid_alternative = row.modelidAlternative;
            m_creatureModels.Set(row.modelid, model);
        }
        loaded = uint32(read.rows.size());
    }

    // post processing
    for (uint32 i = 1; i < m_creatureModels.GetMaxEntry(); ++i)
    {
        CreatureModelInfo const* minfo = m_creatureModels.Find(i);
        if (!minfo)
        {
            continue;
        }

        if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid))
        {
            sLog.outErrorDb("Table `creature_model_info` has model for nonexistent model id (%u).", minfo->modelid);
        }

        if (minfo->gender >= MAX_GENDER)
        {
            sLog.outErrorDb("Table `creature_model_info` has invalid gender (%u) for model id (%u).", uint32(minfo->gender), minfo->modelid);
            m_creatureModels.FindForEdit(minfo->modelid)->gender = GENDER_MALE;
        }

        if (minfo->modelid_other_gender)
        {
            if (minfo->modelid_other_gender == minfo->modelid)
            {
                sLog.outErrorDb("Table `creature_model_info` has redundant modelid_other_gender model (%u) defined for model id %u.", minfo->modelid_other_gender, minfo->modelid);
                m_creatureModels.FindForEdit(minfo->modelid)->modelid_other_gender = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid_other_gender))
            {
                sLog.outErrorDb("Table `creature_model_info` has nonexistent modelid_other_gender model (%u) defined for model id %u.", minfo->modelid_other_gender, minfo->modelid);
                m_creatureModels.FindForEdit(minfo->modelid)->modelid_other_gender = 0;
            }
        }

        if (minfo->modelid_alternative)
        {
            if (minfo->modelid_alternative == minfo->modelid)
            {
                sLog.outErrorDb("Table `creature_model_info` has redundant modelid_alternative model (%u) defined for model id %u.", minfo->modelid_alternative, minfo->modelid);
                m_creatureModels.FindForEdit(minfo->modelid)->modelid_alternative = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid_alternative))
            {
                sLog.outErrorDb("Table `creature_model_info` has nonexistent modelid_alternative model (%u) defined for model id %u.", minfo->modelid_alternative, minfo->modelid);
                m_creatureModels.FindForEdit(minfo->modelid)->modelid_alternative = 0;
            }
        }
    }

    // character races expected have model info data in table
    for (uint32 race = 1; race < sChrRacesStore.GetNumRows(); ++race)
    {
        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
        {
            continue;
        }

        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE))
        {
            continue;
        }

        if (CreatureModelInfo const* minfo = GetCreatureModelInfo(raceEntry->FemaleDisplayID))
        {
            if (minfo->gender != GENDER_FEMALE)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong gender %u for character race %u female model id %u", minfo->gender, race, raceEntry->FemaleDisplayID);
            }

            if (minfo->modelid_other_gender != raceEntry->MaleDisplayID)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong other gender model id %u for character race %u female model id %u", minfo->modelid_other_gender, race, raceEntry->FemaleDisplayID);
            }

            if (minfo->bounding_radius <= 0.0f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong bounding_radius %f for character race %u female model id %u, use %f instead", minfo->bounding_radius, race, raceEntry->FemaleDisplayID, DEFAULT_WORLD_OBJECT_SIZE);
                m_creatureModels.FindForEdit(minfo->modelid)->bounding_radius = DEFAULT_WORLD_OBJECT_SIZE;
            }

            if (minfo->combat_reach != 1.5f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong combat_reach %f for character race %u female model id %u, expected always 1.5f", minfo->combat_reach, race, raceEntry->FemaleDisplayID);
                m_creatureModels.FindForEdit(minfo->modelid)->combat_reach = 1.5f;
            }
        }
        else
        {
            sLog.outErrorDb("Table `creature_model_info` expect have data for character race %u female model id %u", race, raceEntry->FemaleDisplayID);
        }

        if (CreatureModelInfo const* minfo = GetCreatureModelInfo(raceEntry->MaleDisplayID))
        {
            if (minfo->gender != GENDER_MALE)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong gender %u for character race %u male model id %u", minfo->gender, race, raceEntry->MaleDisplayID);
            }

            if (minfo->modelid_other_gender != raceEntry->FemaleDisplayID)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong other gender model id %u for character race %u male model id %u", minfo->modelid_other_gender, race, raceEntry->MaleDisplayID);
            }

            if (minfo->bounding_radius <= 0.0f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong bounding_radius %f for character race %u male model id %u, use %f instead", minfo->bounding_radius, race, raceEntry->FemaleDisplayID, DEFAULT_WORLD_OBJECT_SIZE);
                m_creatureModels.FindForEdit(minfo->modelid)->bounding_radius = DEFAULT_WORLD_OBJECT_SIZE;
            }

            if (minfo->combat_reach != 1.5f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong combat_reach %f for character race %u male model id %u, expected always 1.5f", minfo->combat_reach, race, raceEntry->MaleDisplayID);
                m_creatureModels.FindForEdit(minfo->modelid)->combat_reach = 1.5f;
            }
        }
        else
        {
            sLog.outErrorDb("Table `creature_model_info` expect have data for character race %u male model id %u", race, raceEntry->MaleDisplayID);
        }
    }

    sLog.outString(">> Loaded %u creature model based info", loaded);
    sLog.outString();
}

/**
 * @brief Loads creature spell templates and referenced script bindings.
 */
void ObjectMgr::LoadCreatureModelRace()
{
    m_mCreatureModelRaceMap.clear();                        // can be used for reload

    std::vector<CreatureModelRaceRow> rows = CreatureDataRepository::QueryModelRaces();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded creature_model_race, table is empty!");
        return;
    }


    BarGoLink bar(rows.size());

    uint32 count = 0;

    // model, racemask
    std::map<uint32, uint32> model2raceMask;

    for (CreatureModelRaceRow const& row : rows)
    {
        bar.step();

        CreatureModelRace raceData;

        raceData.modelid            = row.modelid;
        raceData.racemask           = row.racemask;
        raceData.creature_entry     = row.creatureEntry;
        raceData.modelid_racial     = row.modelidRacial;

        if (!sCreatureDisplayInfoStore.LookupEntry(raceData.modelid))
        {
            sLog.outErrorDb("Table `creature_model_race` has model for nonexistent model id (%u), skipping", raceData.modelid);
            continue;
        }

        if (!GetCreatureModelInfo(raceData.modelid))
        {
            sLog.outErrorDb("Table `creature_model_race` modelid %u does not exist in creature_model_info, skipping", raceData.modelid);
            continue;
        }

        if (!raceData.racemask)
        {
            sLog.outErrorDb("Table `creature_model_race` modelid %u has no racemask defined, skipping", raceData.modelid);
            continue;
        }

        if (!(raceData.racemask & RACEMASK_ALL_PLAYABLE))
        {
            sLog.outErrorDb("Table `creature_model_race` modelid %u include invalid racemask, skipping", raceData.modelid);
            continue;
        }

        std::map<uint32, uint32>::const_iterator model2Race = model2raceMask.find(raceData.modelid);

        // can't have same mask for same model several times
        if (model2Race != model2raceMask.end())
        {
            if (model2Race->second & raceData.racemask)
            {
                sLog.outErrorDb("Table `creature_model_race` modelid %u with racemask %u has mask already included for same modelid, skipping", raceData.modelid, raceData.racemask);
                continue;
            }
        }

        model2raceMask[raceData.modelid] |= raceData.racemask;

        // creature_entry is the prefered way
        if (raceData.creature_entry)
        {
            if (raceData.modelid_racial)
                sLog.outErrorDb("Table `creature_model_race` modelid %u has modelid_racial for modelid %u but a creature_entry are already defined, modelid_racial will never be used.", raceData.modelid, raceData.modelid_racial);

            if (!ObjectMgr::GetCreatureTemplate(raceData.creature_entry))
            {
                sLog.outErrorDb("Table `creature_model_race` modelid %u has creature_entry for nonexistent creature_template (%u), skipping", raceData.modelid, raceData.creature_entry);
                continue;
            }
        }
        else if (raceData.modelid_racial)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(raceData.modelid_racial))
            {
                sLog.outErrorDb("Table `creature_model_race` modelid %u has modelid_racial for nonexistent model id (%u), skipping", raceData.modelid, raceData.modelid_racial);
                continue;
            }

            if (!GetCreatureModelInfo(raceData.modelid_racial))
            {
                sLog.outErrorDb("Table `creature_model_race` modelid %u has modelid_racial %u, but are not defined in creature_model_info, skipping", raceData.modelid, raceData.modelid_racial);
                continue;
            }
        }
        else
        {
            sLog.outErrorDb("Table `creature_model_race` modelid %u does not have either creature_entry or modelid_racial defined, skipping", raceData.modelid);
            continue;
        }

        m_mCreatureModelRaceMap.insert(CreatureModelRaceMap::value_type(raceData.modelid, raceData));

        ++count;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u creature_model_race entries", count);
}

/**
 * @brief Loads creature spawn records and validates their database data.
 */
void ObjectMgr::LoadCreatures()
{
    uint32 count = 0;
    std::vector<CreatureSpawnDataRow> rows = SpawnRepository::QueryCreatureSpawns();

    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 creature. DB table `creature` is empty.");
        sLog.outString();
        return;
    }

    // build single time for check creature data
    std::set<uint32> difficultyCreatures[MAX_DIFFICULTY - 1];
    for (uint32 i = 0; i < sObjectMgr.GetCreatureTemplates().GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(i))
            for (uint32 diff = 0; diff < MAX_DIFFICULTY - 1; ++diff)
            {
                if (cInfo->DifficultyEntry[diff])
                {
                    difficultyCreatures[diff].insert(cInfo->DifficultyEntry[diff]);
                }
            }
    }

    // build single time for check spawnmask
    std::map<uint32, uint32> spawnMasks;
    for (uint32 i = 0; i < sMapStore.GetNumRows(); ++i)
    {
        if (sMapStore.LookupEntry(i))
            for (int k = 0; k < MAX_DIFFICULTY; ++k)
            {
                if (GetMapDifficultyData(i, Difficulty(k)))
                {
                    spawnMasks[i] |= (1 << k);
                }
            }
    }
    // Map 0 was removed from dbc as of 4.x.x
    spawnMasks[0] = 1 << REGULAR_DIFFICULTY;

    BarGoLink bar(rows.size());

    const uint32 lwAnchorMask = sGameConfig.getConfig(CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK);
    uint32 lwWorldBossLeaderCount = 0;
    uint32 lwFlightMasterCount = 0;
    uint32 lwSettlementDefenderCount = 0;
    uint32 lwAnchorTotal = 0;

    for (CreatureSpawnDataRow const& row : rows)
    {
        bar.step();

        uint32 guid         = row.guid;
        uint32 entry        = row.id;

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_CREATURE_SPAWN, guid))
        {
            sLog.outDebug("Creature guid %u (entry %u) spawning is disabled.", guid, entry);
            continue;
        }

        CreatureInfo const* cInfo = GetCreatureTemplate(entry);
        if (!cInfo)
        {
            sLog.outErrorDb("Table `creature` has creature (GUID: %u) with non existing creature entry %u, skipped.", guid, entry);
            continue;
        }

        CreatureData& data = mCreatureDataMap[guid];

        data.id                 = entry;
        data.mapid              = row.map;
        data.modelid_override   = row.modelid;
        data.equipmentId        = row.equipmentId;
        data.posX               = row.positionX;
        data.posY               = row.positionY;
        data.posZ               = row.positionZ;
        data.orientation        = row.orientation;
        data.spawntimesecs      = row.spawntimesecs;
        data.spawndist          = row.spawndist;
        data.currentwaypoint    = row.currentwaypoint;
        data.curhealth          = row.curhealth;
        data.curmana            = row.curmana;
        data.is_dead            = row.deathState;
        data.movementType       = row.movementType;
        data.spawnMask          = row.spawnMask;
        data.phaseMask          = row.phaseMask;
        int16 gameEvent         = row.gameEvent;
        int16 GuidPoolId        = row.guidPoolId;
        int16 EntryPoolId       = row.entryPoolId;

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u) that spawned at nonexistent map (Id: %u), skipped.", guid, data.mapid);
            continue;
        }

        if (data.spawnMask & ~spawnMasks[data.mapid])
            sLog.outErrorDb("Table `creature` have creature (GUID: %u) that have wrong spawn mask %u including not supported difficulty modes for map (Id: %u).", guid, data.spawnMask, data.mapid);

        bool ok = true;
        for (uint32 diff = 0; diff < MAX_DIFFICULTY - 1 && ok; ++diff)
        {
            if (difficultyCreatures[diff].find(data.id) != difficultyCreatures[diff].end())
            {
                sLog.outErrorDb("Table `creature` have creature (GUID: %u) that listed as difficulty %u template (entry: %u) in `creature_template`, skipped.",
                                guid, diff + 1, data.id);
                ok = false;
            }
        }
        if (!ok)
        {
            continue;
        }

        if (data.modelid_override > 0 && !sCreatureDisplayInfoStore.LookupEntry(data.modelid_override))
        {
            sLog.outErrorDb("Table `creature` GUID %u (entry %u) has model for nonexistent model id (%u), set to 0.", guid, data.id, data.modelid_override);
            data.modelid_override = 0;
        }

        if (data.equipmentId > 0)                           // -1 no equipment, 0 use default
        {
            if (!GetEquipmentInfo(data.equipmentId))
            {
                sLog.outErrorDb("Table `creature` have creature (Entry: %u) with equipment_id %u not found in table `creature_equip_template`, set to no equipment.", data.id, data.equipmentId);
                data.equipmentId = -1;
            }
        }

        if (cInfo->RegenerateStats & REGEN_FLAG_HEALTH && data.curhealth < cInfo->MinLevelHealth)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`RegenerateStats` & REGEN_FLAG_HEALTH and low current health (%u), `creature_template`.`MinLevelHealth`=%u.", guid, data.id, data.curhealth, cInfo->MinLevelHealth);
            data.curhealth = cInfo->MinLevelHealth;
        }

        if (cInfo->ExtraFlags & CREATURE_FLAG_EXTRA_INSTANCE_BIND)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`ExtraFlags` including CREATURE_FLAG_EXTRA_INSTANCE_BIND (%u) but creature are not in instance.",
                                guid, data.id, CREATURE_FLAG_EXTRA_INSTANCE_BIND);
        }

        if (cInfo->ExtraFlags & CREATURE_FLAG_EXTRA_AGGRO_ZONE)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`ExtraFlags` including CREATURE_FLAG_EXTRA_AGGRO_ZONE (%u) but creature are not in instance.",
                                guid, data.id, CREATURE_FLAG_EXTRA_AGGRO_ZONE);
        }

        if (data.curmana < cInfo->MinLevelMana)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with low current mana (%u), `creature_template`.`MinLevelMana`=%u.", guid, data.id, data.curmana, cInfo->MinLevelMana);
            data.curmana = cInfo->MinLevelMana;
        }

        if (data.spawndist < 0.0f)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `spawndist`< 0, set to 0.", guid, data.id);
            data.spawndist = 0.0f;
        }
        else if (data.movementType == CREATURE_MOVEMENT_RANDOM)
        {
            if (data.spawndist == 0.0f)
            {
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `MovementType`=1 (random movement) but with `spawndist`=0, replace by idle movement type (0).", guid, data.id);
                data.movementType = CREATURE_MOVEMENT_IDLE;
            }
        }
        else if (data.movementType == CREATURE_MOVEMENT_IDLE)
        {
            if (data.spawndist != 0.0f)
            {
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `MovementType`=0 (idle) have `spawndist`<>0, set to 0.", guid, data.id);
                data.spawndist = 0.0f;
            }
        }

        if (data.phaseMask == 0)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `phaseMask`=0 (not visible for anyone), set to 1.", guid, data.id);
            data.phaseMask = 1;
        }

        if (gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0) // if not this is to be managed by GameEvent System or Pool system
        {
            AddCreatureToGrid(guid, &data);

            const bool lwIsWaypoint = (data.movementType == CREATURE_MOVEMENT_WAYPOINT);
            uint32 lwCats = (GetLivingWorldAnchorCategories(cInfo, mapEntry)
                          |  GetLivingWorldDefenderCategory(cInfo, mapEntry, lwIsWaypoint)) & lwAnchorMask;
            if ((cInfo->ExtraFlags & CREATURE_FLAG_EXTRA_ACTIVE) || lwCats != 0)
            {
                BASIC_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Adding `creature` with Active Flag: Map: %u, Guid %u", data.mapid, guid);
                m_activeCreatures.insert(ActiveCreatureGuidsOnMap::value_type(data.mapid, guid));

                if (lwCats != 0)
                {
                    ++lwAnchorTotal;
                    if (lwCats & LW_ANCHOR_WORLD_BOSS_OR_LEADER)
                    {
                        ++lwWorldBossLeaderCount;
                    }
                    if (lwCats & LW_ANCHOR_FLIGHT_MASTER)
                    {
                        ++lwFlightMasterCount;
                    }
                    if (lwCats & LW_ANCHOR_SETTLEMENT_DEFENDER)
                    {
                        ++lwSettlementDefenderCount;
                    }
                }
            }
        }

        ++count;
    }

    sLog.outString(">> Loaded %zu creatures", mCreatureDataMap.size());
    sLog.outString("[LivingWorld] anchor policy mask=0x%X: world-boss/leaders=%u, flight-masters=%u, settlement-defenders=%u, total-anchors=%u (continent-only; instance spawns excluded)",
        lwAnchorMask, lwWorldBossLeaderCount, lwFlightMasterCount, lwSettlementDefenderCount, lwAnchorTotal);
    sLog.outString();
}

/**
 * @brief Adds a creature spawn GUID to the grid lookup for its map cell.
 *
 * @param guid The creature spawn GUID.
 * @param data The creature spawn data.
 */
void ObjectMgr::AddCreatureToGrid(uint32 guid, CreatureData const* data)
{
    uint8 mask = data->spawnMask;
    for (uint8 i = 0; mask != 0; ++i, mask >>= 1)
    {
        if (mask & 1)
        {
            CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
            uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

            CellObjectGuids& cell_guids = mMapObjectGuids[MAKE_PAIR64(data->mapid, i)][cell_id];
            cell_guids.creatures.insert(guid);
        }
    }
}

/**
 * @brief Removes a creature spawn GUID from the grid lookup for its map cell.
 *
 * @param guid The creature spawn GUID.
 * @param data The creature spawn data.
 */
void ObjectMgr::RemoveCreatureFromGrid(uint32 guid, CreatureData const* data)
{
    uint8 mask = data->spawnMask;
    for (uint8 i = 0; mask != 0; ++i, mask >>= 1)
    {
        if (mask & 1)
        {
            CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
            uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

            CellObjectGuids& cell_guids = mMapObjectGuids[MAKE_PAIR64(data->mapid, i)][cell_id];
            cell_guids.creatures.erase(guid);
        }
    }
}

uint32 ChooseCreatureDisplayId(CreatureInfo const* cinfo, CreatureData const* data, GameEventCreatureData const* eventData)
{
    // Use creature event model explicit, override any other static models
    if (eventData && eventData->modelid)
    {
        return eventData->modelid;
    }

    // Use creature model explicit, override template (creature.modelid)
    if (data && data->modelid_override)
    {
        return data->modelid_override;
    }

    // use defaults from the template
    uint32 display_id = 0;

    // models may be categorized as (in this order):
    // if mod4 && mod3 && mod2 && mod1  use any, by 25%-chance (other gender is selected and replaced after this function)
    // if mod3 && mod2 && mod1          use mod3 unless mod2 has modelid_alt_model (then all by 33%-chance)
    // if mod2                          use mod2 unless mod2 has modelid_alt_model (then both by 50%-chance)
    // if mod1                          use mod1

    // The follow decision tree needs to be updated if MAX_CREATURE_MODEL is changed.
    static_assert(MAX_CREATURE_MODEL == 4, "Need to update model selection code for new or removed model fields");

    // model selected here may be replaced with other_gender using own function

    if (cinfo->ModelId[3] && cinfo->ModelId[2] && cinfo->ModelId[1] && cinfo->ModelId[0])
    {
        display_id = cinfo->ModelId[urand(0, 3)];
    }
    else if (cinfo->ModelId[2] && cinfo->ModelId[1] && cinfo->ModelId[0])
    {
        uint32 modelid_tmp = sObjectMgr.GetCreatureModelAlternativeModel(cinfo->ModelId[1]);
        display_id = modelid_tmp ? cinfo->ModelId[urand(0, 2)] : cinfo->ModelId[2];
    }
    else if (cinfo->ModelId[1])
    {
        // We use this to eliminate invisible models vs. "dummy" models (infernals, etc).
        // Where it's expected to select one of two, model must have a alternative model defined (alternative model is normally the same as defined in ModelId1).
        // Same pattern is used in the above model selection, but the result may be ModelId3 and not ModelId2 as here.
        uint32 modelid_tmp = sObjectMgr.GetCreatureModelAlternativeModel(cinfo->ModelId[1]);
        display_id = modelid_tmp ? cinfo->ModelId[urand(0, 1)] : cinfo->ModelId[1];
    }
    else if (cinfo->ModelId[0])
    {
        display_id = cinfo->ModelId[0];
    }

    // fail safe, we use creature entry 1 and make error
    if (!display_id)
    {
        sLog.outErrorDb("Call customer support, ChooseDisplayId can not select native model for creature entry %u, model from creature entry 1 will be used instead.", cinfo->Entry);

        if (const CreatureInfo* creatureDefault = ObjectMgr::GetCreatureTemplate(1))
        {
            display_id = creatureDefault->ModelId[0];
        }
    }

    return display_id;
}

void ObjectMgr::LoadVehicleAccessory()
{
    TableRead<VehicleAccessoryRow> read = VehicleAccessoryRepository::QueryVehicleAccessories();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `vehicle_accessory` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    uint32 loaded = 0;
    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("vehicle_accessory table is empty!\n");
    }
    else
    {
        m_vehicleAccessories.Clear();
        BarGoLink bar(read.rows.size());
        for (VehicleAccessoryRow const& row : read.rows)
        {
            bar.step();
            VehicleAccessory accessory;
            accessory.vehicleEntry = row.vehicleEntry;
            accessory.seatId = row.seat;
            accessory.passengerEntry = row.accessoryEntry;
            m_vehicleAccessories.Add(accessory.vehicleEntry, accessory);
        }
        loaded = uint32(read.rows.size());
    }

    sLog.outString(">> Loaded %u vehicle accessories", loaded);
    sLog.outString();

    // Check content: every row read, in the order read, the rows of a vehicle already erased included
    for (VehicleAccessoryRow const& row : read.rows)
    {
        if (!ObjectMgr::GetCreatureTemplate(row.vehicleEntry))
        {
            sLog.outErrorDb("Table `vehicle_accessory` has entry (vehicle entry: %u, seat %u, passenger %u) where vehicle_entry is invalid, skip vehicle.", row.vehicleEntry, row.seat, row.accessoryEntry);
            m_vehicleAccessories.Erase(row.vehicleEntry);
            continue;
        }
        if (!ObjectMgr::GetCreatureTemplate(row.accessoryEntry))
        {
            sLog.outErrorDb("Table `vehicle_accessory` has entry (vehicle entry: %u, seat %u, passenger %u) where accessory_entry is invalid, skip vehicle.", row.vehicleEntry, row.seat, row.accessoryEntry);
            m_vehicleAccessories.Erase(row.vehicleEntry);
            continue;
        }
        if (row.seat >= MAX_VEHICLE_SEAT)
        {
            sLog.outErrorDb("Table `vehicle_accessory` has entry (vehicle entry: %u, seat %u, passenger %u) where seat is invalid (must be between 0 and %u), skip vehicle.", row.vehicleEntry, row.seat, row.accessoryEntry, MAX_VEHICLE_SEAT - 1);
            m_vehicleAccessories.Erase(row.vehicleEntry);
            continue;
        }
    }
}
