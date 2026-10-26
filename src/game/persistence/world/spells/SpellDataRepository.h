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

#ifndef MANGOS_H_SPELLDATAREPOSITORY
#define MANGOS_H_SPELLDATAREPOSITORY

#include "Platform/Define.h"

#include <string>
#include <vector>

/// One row of a table of spell references the checks validate.
struct UsedSpellRow
{
    uint32 spellid = 0;
    int32 spellFamilyName = 0;
    uint64 spellFamilyMaskA = 0;
    uint32 spellFamilyMaskB = 0;
    int32 spellIcon = 0;
    int32 spellVisual = 0;
    int32 spellCategory = 0;
    int32 effectType = 0;
    int32 effectAura = 0;
    int32 effectIdx = 0;
    std::string name;
    std::string code;
};

/// One `spell_area` row.
struct SpellAreaRow
{
    uint32 spell = 0;
    uint32 area = 0;
    uint32 questStart = 0;
    bool questStartActive = false;
    uint32 questEnd = 0;
    uint16 conditionId = 0;
    int32 auraSpell = 0;
    uint32 racemask = 0;
    uint8 gender = 0;
    bool autocast = false;
};

/// One `spell_chain` row.
struct SpellChainRow
{
    uint32 spellId = 0;
    uint32 prevSpell = 0;
    uint32 firstSpell = 0;
    uint8 rank = 0;
    uint32 reqSpell = 0;
};

/// One `spell_learn_spell` row.
struct LearnSpellRow
{
    uint32 entry = 0;
    uint32 spellID = 0;
    bool active = false;
};

/// One `spell_target_position` row.
struct SpellTargetPositionRow
{
    uint32 id = 0;
    uint32 targetMap = 0;
    float targetPositionX = 0.0f;
    float targetPositionY = 0.0f;
    float targetPositionZ = 0.0f;
    float targetOrientation = 0.0f;
};

/// One `spell_bonus_data` row.
struct SpellBonusRow
{
    uint32 entry = 0;
    float directBonus = 0.0f;
    float dotBonus = 0.0f;
    float apBonus = 0.0f;
    float apDotBonus = 0.0f;
};

/// One `spell_elixir` row.
struct SpellElixirRow
{
    uint32 entry = 0;
    uint8 mask = 0;
};

/// One `spell_threat` row.
struct SpellThreatRow
{
    uint32 entry = 0;
    uint16 threat = 0;
    float multiplier = 0.0f;
    float apBonus = 0.0f;
};

/// One `spell_pet_auras` row.
struct SpellPetAuraRow
{
    uint32 spell = 0;
    uint32 effectId = 0;
    uint32 pet = 0;
    uint32 aura = 0;
};

enum
{
    /// The family masks a `spell_proc_event` row holds per part: one per spell effect.
    SPELL_PROC_EVENT_ROW_MASKS = 3
};

/// One `spell_proc_event` row; the A, B and C parts of each effect's family mask.
struct SpellProcEventRow
{
    uint32 entry = 0;
    uint32 schoolMask = 0;
    uint32 spellFamilyName = 0;
    uint32 spellFamilyMaskA[SPELL_PROC_EVENT_ROW_MASKS] = {};
    uint32 spellFamilyMaskB[SPELL_PROC_EVENT_ROW_MASKS] = {};
    uint32 spellFamilyMaskC[SPELL_PROC_EVENT_ROW_MASKS] = {};
    uint32 procFlags = 0;
    uint32 procEx = 0;
    float ppmRate = 0.0f;
    float customChance = 0.0f;
    uint32 cooldown = 0;
};

/// One `spell_proc_item_enchant` row.
struct SpellProcItemEnchantRow
{
    uint32 entry = 0;
    float ppmRate = 0.0f;
};

/// The spell data on the world database: `SpellMgr`'s loads at startup, on a reload and for the
/// debug check of a spell table, each run synchronously and read into rows.
namespace SpellDataRepository
{
    /// Selects every row of a table of spell references the checks validate.
    std::vector<UsedSpellRow> QueryUsedSpells(char const* table);

    /// Selects every area-bound spell (`spell_area`).
    std::vector<SpellAreaRow> QuerySpellAreas();

    /// Selects every spell rank chain (`spell_chain`).
    std::vector<SpellChainRow> QuerySpellChains();

    /// Selects every spell a spell teaches (`spell_learn_spell`).
    std::vector<LearnSpellRow> QueryLearnSpells();

    /// Selects every teleport target position (`spell_target_position`).
    std::vector<SpellTargetPositionRow> QueryTargetPositions();

    /// Selects every spell bonus coefficient override (`spell_bonus_data`).
    std::vector<SpellBonusRow> QueryBonuses();

    /// Selects every elixir mask (`spell_elixir`).
    std::vector<SpellElixirRow> QueryElixirs();

    /// Selects every spell threat override (`spell_threat`).
    std::vector<SpellThreatRow> QueryThreats();

    /// Selects every pet aura (`spell_pet_auras`).
    std::vector<SpellPetAuraRow> QueryPetAuras();

    /// Selects every proc event (`spell_proc_event`).
    std::vector<SpellProcEventRow> QueryProcEvents();

    /// Selects every item enchantment's procs per minute (`spell_proc_item_enchant`).
    std::vector<SpellProcItemEnchantRow> QueryProcItemEnchants();
}

#endif
