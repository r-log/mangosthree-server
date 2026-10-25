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

#ifndef MANGOS_H_CHARACTERSAVEREPOSITORY
#define MANGOS_H_CHARACTERSAVEREPOSITORY

#include <string>
#include <vector>
#include "Platform/Define.h"

/// The sizes of the saved rows' arrays: the stored powers, an aura's effects, the base stats, the
/// spell schools (armor first) and an equipment set's items.
enum
{
    CHARACTER_SAVE_ROW_POWERS        = 5,
    CHARACTER_AURA_ROW_EFFECTS       = 3,
    CHARACTER_STATS_ROW_STATS        = 5,
    CHARACTER_STATS_ROW_SCHOOLS      = 7,
    EQUIPMENT_SET_ROW_ITEMS          = 19
};

/// A character's saved row, in the order of the INSERT's columns.
struct CharacterSaveRow
{
    uint32 guid = 0;
    uint32 account = 0;
    std::string name;
    uint8 race = 0;
    uint8 playerClass = 0;
    uint8 gender = 0;
    uint32 level = 0;
    uint32 xp = 0;
    uint64 money = 0;
    uint32 playerBytes = 0;
    uint32 playerBytes2 = 0;
    uint32 playerFlags = 0;
    uint32 map = 0;
    uint32 dungeonDifficulty = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    std::string taxiMask;
    uint32 online = 0;
    uint32 cinematic = 0;
    uint32 totalTime = 0;
    uint32 levelTime = 0;
    float restBonus = 0.0f;
    uint64 logoutTime = 0;
    uint32 isLogoutResting = 0;
    uint32 resetTalentsCost = 0;
    uint64 resetTalentsTime = 0;
    std::string primaryTrees;
    float transX = 0.0f;
    float transY = 0.0f;
    float transZ = 0.0f;
    float transO = 0.0f;
    uint32 transGuid = 0;
    uint32 extraFlags = 0;
    uint32 stableSlots = 0;
    uint32 atLogin = 0;
    uint32 zone = 0;
    uint64 deathExpireTime = 0;
    std::string taxiPath;
    uint32 totalKills = 0;
    uint16 todayKills = 0;
    uint16 yesterdayKills = 0;
    uint32 chosenTitle = 0;
    uint32 watchedFaction = 0;
    uint8 drunk = 0;
    uint32 health = 0;
    uint32 power[CHARACTER_SAVE_ROW_POWERS] = {};
    uint32 specCount = 0;
    uint32 activeSpec = 0;
    std::string exploredZones;
    std::string equipmentCache;
    std::string knownTitles;
    uint32 actionBars = 0;
    uint8 slot = 0;
    uint32 createdDate = 0;
};

/// A character aura's saved row, in the order of the INSERT's columns after the owner.
struct CharacterAuraSaveRow
{
    uint64 casterGuid = 0;
    uint32 itemGuid = 0;
    uint32 spell = 0;
    uint32 stackCount = 0;
    uint8 remainCharges = 0;
    int32 basePoints[CHARACTER_AURA_ROW_EFFECTS] = {};
    uint32 periodicTime[CHARACTER_AURA_ROW_EFFECTS] = {};
    int32 maxDuration = 0;
    int32 remainTime = 0;
    uint32 effIndexMask = 0;
};

/// A raid-frame profile's saved row, in the order of the INSERT's columns after the owner.
struct CUFProfileSaveRow
{
    uint8 id = 0;
    std::string name;
    uint16 frameHeight = 0;
    uint16 frameWidth = 0;
    uint8 sortBy = 0;
    uint8 healthText = 0;
    uint32 boolOptions = 0;
    uint8 topPoint = 0;
    uint8 bottomPoint = 0;
    uint8 leftPoint = 0;
    uint16 topOffset = 0;
    uint16 bottomOffset = 0;
    uint16 leftOffset = 0;
};

/// A character's saved stats, in the order of the INSERT's columns after the owner.
struct CharacterStatsSaveRow
{
    uint32 maxHealth = 0;
    uint32 maxPower[CHARACTER_SAVE_ROW_POWERS] = {};
    float stat[CHARACTER_STATS_ROW_STATS] = {};
    uint32 resistance[CHARACTER_STATS_ROW_SCHOOLS] = {};
    float blockPct = 0.0f;
    float dodgePct = 0.0f;
    float parryPct = 0.0f;
    float critPct = 0.0f;
    float rangedCritPct = 0.0f;
    float spellCritPct = 0.0f;
    uint32 attackPower = 0;
    uint32 rangedAttackPower = 0;
    uint32 spellPower = 0;
};

/// An equipment set's saved row.
struct EquipmentSetSaveRow
{
    uint64 setGuid = 0;
    uint32 index = 0;
    std::string name;
    std::string iconName;
    uint32 ignoreMask = 0;
    uint32 items[EQUIPMENT_SET_ROW_ITEMS] = {};
};

/// A character's saved battleground return point, in the order of the INSERT's columns after the
/// owner.
struct BattlegroundDataSaveRow
{
    uint32 instanceId = 0;
    uint32 team = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float o = 0.0f;
    uint32 map = 0;
    uint32 taxiStart = 0;
    uint32 taxiEnd = 0;
    uint32 mountSpell = 0;
};

/// A character's save (`characters` and the tables it saves on its own: `character_action`,
/// `character_aura`, `character_cuf_profiles`, `character_skills`, `character_spell`,
/// `character_stats`, `character_equipmentsets`, `character_battleground_data`). Every write is
/// queued on the character database, and one called inside CharacterTransaction joins it.
namespace CharacterSaveRepository
{
    /// Writes a character's row over the old one: a DELETE, then the INSERT, the name and the
    /// strings bound.
    void ReplaceCharacter(CharacterSaveRow const& row);

    /// Sets a character's money.
    void SetMoney(uint64 money, uint32 guid);

    /// Sets an offline character's position and zone and clears its transport and flight path;
    /// returns the statement's text.
    std::string SetPosition(uint32 guid, uint32 map, float x, float y, float z, float o, uint32 zone);

    /// Each writes one action button of a talent spec.
    void InsertAction(uint32 guid, uint32 spec, uint32 button, uint32 action, uint32 type);
    void UpdateAction(uint32 guid, uint32 spec, uint32 button, uint32 action, uint32 type);
    void DeleteAction(uint32 guid, uint32 spec, uint32 button);

    /// Deletes a character's auras, then inserts the ones handed, in order, through one statement.
    void ReplaceAuras(uint32 guid, std::vector<CharacterAuraSaveRow> const& auras);

    /// Deletes a character's raid-frame profiles, then inserts the ones handed, in order.
    void ReplaceCUFProfiles(uint32 guid, std::vector<CUFProfileSaveRow> const& profiles);

    /// Each writes one skill.
    void InsertSkill(uint32 guid, uint32 skill, uint16 value, uint16 max);
    void UpdateSkill(uint32 guid, uint32 skill, uint16 value, uint16 max);
    void DeleteSkill(uint32 guid, uint32 skill);

    /// Each writes one learned spell.
    void InsertSpell(uint32 guid, uint32 spell, uint8 active, uint8 disabled);
    void DeleteSpell(uint32 guid, uint32 spell);

    /// Writes a character's stats over the old ones.
    void ReplaceStats(uint32 guid, CharacterStatsSaveRow const& row);

    /// Each writes one equipment set, the names bound; a set is deleted by its guid alone.
    void InsertEquipmentSet(uint32 guid, EquipmentSetSaveRow const& row);
    void UpdateEquipmentSet(uint32 guid, EquipmentSetSaveRow const& row);
    void DeleteEquipmentSet(uint64 setGuid);

    /// Deletes a character's battleground return point, then inserts the one handed, if any.
    void ReplaceBattlegroundData(uint32 guid, BattlegroundDataSaveRow const* row);
}

#endif
