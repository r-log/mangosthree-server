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
 * @file CharacterDatabaseCleaner.cpp
 * @brief Character database cleanup utility
 *
 * This file implements the CharacterDatabaseCleaner which removes invalid
 * or orphaned data from the character database. It validates data against
 * DBC stores to ensure only legitimate entries remain.
 *
 * Cleanup operations:
 * - Remove invalid skills (not in SkillLine.dbc)
 * - Remove invalid spells (not in Spell.dbc)
 *
 * The cleaner uses flags stored in the `saved_variables` table to determine
 * which cleanup operations are needed. This allows incremental cleaning
 * across server restarts. A change of the loaded DBC client build against
 * `saved_variables`.`last_cleaned_dbc_build` schedules a full cleanup.
 *
 * @see CharacterDatabaseCleaner for the cleaner interface
 * @see CLEANING_FLAG_* constants for available operations
 */

#include "Platform/Define.h"
#include <sstream>
#include "CharacterDatabaseCleaner.h"
#include "data/GameConfig.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "ProgressBar.h"
#include "persistence/characters/CharacterCleanupRepository.h"
#include "persistence/characters/SavedVariablesRepository.h"

/**
 * @brief Main database cleanup entry point
 *
 * Performs database cleaning based on configuration and stored flags:
 * 1. Checks if cleaning is enabled in config
 * 2. Reads cleaning flags from saved_variables table
 * 3. Schedules a full cleanup when the loaded DBC build differs from
 *    saved_variables.last_cleaned_dbc_build (DBC data was swapped)
 * 4. Executes appropriate cleanup routines
 * 5. Resets cleaning flags and records the DBC build when complete
 *
 * @note Can be disabled via CONFIG_BOOL_CLEAN_CHARACTER_DB
 */
void CharacterDatabaseCleaner::CleanDatabase()
{
    // Skip if disabled in config
    if (!sGameConfig.getConfig(CONFIG_BOOL_CLEAN_CHARACTER_DB))
    {
        return;
    }

    // Check which cleanups are needed
    std::optional<uint32> cleaningFlags = SavedVariablesRepository::QueryCleaningFlags();
    if (!cleaningFlags)
    {
        return;
    }
    uint32 flags = *cleaningFlags;

    // Orphaned rows appear when the extracted DBC data is swapped for
    // another client build, so a build change schedules a full cleanup.
    bool hasBuildColumn = false;
    uint32 const dbcBuild = GetDBCLoadedBuild();
    if (std::optional<uint32> lastCleaned = SavedVariablesRepository::QueryLastCleanedDbcBuild())
    {
        hasBuildColumn = true;
        uint32 lastCleanedBuild = *lastCleaned;

        if (dbcBuild && lastCleanedBuild != dbcBuild)
        {
            sLog.outString("DBC build changed (%u -> %u), scheduling character database cleanup.", lastCleanedBuild, dbcBuild);
            flags |= CLEANING_FLAG_ALL;
        }
    }
    else
    {
        sLog.outErrorDb("saved_variables.last_cleaned_dbc_build is missing, DBC build-change cleanup disabled. Please apply the character database updates.");
    }

    if (!flags)
    {
        return;
    }

    sLog.outString("Cleaning character database...");

    // clean up
    if (flags & CLEANING_FLAG_ACHIEVEMENT_PROGRESS)
    {
        CleanCharacterAchievementProgress();
    }

    // Execute cleanup based on flags
    if (flags & CLEANING_FLAG_SKILLS)
    {
        CleanCharacterSkills();
    }
    if (flags & CLEANING_FLAG_SPELLS)
    {
        CleanCharacterSpell();
    }
    if (flags & CLEANING_FLAG_TALENTS)
    {
        CleanCharacterTalent();
    }

    // Only reset the flags and record the build after a completed run so
    // an interrupted cleanup is retried on the next startup.
    if (hasBuildColumn)
    {
        SavedVariablesRepository::FinishCleaning(dbcBuild);
    }
    else
    {
        SavedVariablesRepository::FinishCleaningWithoutBuild();
    }
}

/**
 * @brief Validate and remove invalid entries from a table
 * @param column Column name containing the ID to check
 * @param table Table name to clean
 * @param check Validation function returning true if ID is valid
 *
 * Generic cleanup helper that:
 * 1. Queries all distinct values in the specified column
 * 2. Validates each value using the provided check function
 * 3. Deletes rows with invalid IDs in a single DELETE statement
 *
 * Uses a progress bar for visual feedback during long operations.
 */
void CharacterDatabaseCleaner::CheckUnique(const char* column, const char* table, bool (*check)(uint32))
{
    std::vector<uint32> ids = CharacterCleanupRepository::QueryDistinct(column, table);
    if (ids.empty())
    {
        sLog.outString("Table %s is empty.", table);
        return;
    }

    std::vector<uint32> invalid;
    BarGoLink bar(ids.size());
    for (uint32 id : ids)
    {
        bar.step();

        if (!check(id))
        {
            invalid.push_back(id);
        }
    }

    if (!invalid.empty())
    {
        CharacterCleanupRepository::DeleteRowsWithIds(table, column, invalid);
    }
}

bool CharacterDatabaseCleaner::AchievementProgressCheck(uint32 criteria)
{
    return sAchievementCriteriaStore.LookupEntry(criteria);
}

void CharacterDatabaseCleaner::CleanCharacterAchievementProgress()
{
    CheckUnique("criteria", "character_achievement_progress", &AchievementProgressCheck);
}

/**
 * @brief Check if a skill is valid
 * @param skill Skill ID to validate
 * @return true if skill exists in DBC
 *
 * Validates against SkillLine.dbc (sSkillLineStore).
 */
bool CharacterDatabaseCleaner::SkillCheck(uint32 skill)
{
    return sSkillLineStore.LookupEntry(skill);
}

/**
 * @brief Clean invalid character skills
 *
 * Removes skills from character_skills table that don't exist
 * in the SkillLine.dbc file.
 */
void CharacterDatabaseCleaner::CleanCharacterSkills()
{
    CheckUnique("skill", "character_skills", &SkillCheck);
}

/**
 * @brief Check if a spell is valid
 * @param spell_id Spell ID to validate
 * @return true if spell exists in DBC
 *
 * Validates against Spell.dbc (sSpellStore).
 */
bool CharacterDatabaseCleaner::SpellCheck(uint32 spell_id)
{
    return sSpellStore.LookupEntry(spell_id) && !GetTalentSpellPos(spell_id);
}

/**
 * @brief Clean invalid character spells
 *
 * Removes spells from character_spell table that don't exist
 * in the Spell.dbc file.
 */
void CharacterDatabaseCleaner::CleanCharacterSpell()
{
    CheckUnique("spell", "character_spell", &SpellCheck);
}

bool CharacterDatabaseCleaner::TalentCheck(uint32 talent_id)
{
    TalentEntry const* talentInfo = sTalentStore.LookupEntry(talent_id);
    if (!talentInfo)
    {
        return false;
    }

    return sTalentTabStore.LookupEntry(talentInfo->TabID);
}

void CharacterDatabaseCleaner::CleanCharacterTalent()
{
    CharacterCleanupRepository::DeleteTalentsPastLimits(MAX_TALENT_SPEC_COUNT, MAX_TALENT_RANK);

    CheckUnique("talent_id", "character_talent", &TalentCheck);
}
