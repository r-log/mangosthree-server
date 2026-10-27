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

#ifndef MANGOS_H_SPELLTEMPLATEREPOSITORY
#define MANGOS_H_SPELLTEMPLATEREPOSITORY

#include "Platform/Define.h"
#include "persistence/TableRead.h"

#include <string>

class Field;

/// One `spell_template` row: a server-side spell's id, attributes, proc, duration, its one effect and its comment.
struct SpellTemplateRow
{
    uint32 id = 0;
    uint32 attributes = 0;
    uint32 attributesEx = 0;
    uint32 attributesEx2 = 0;
    uint32 attributesEx3 = 0;
    uint32 procFlags = 0;
    uint32 procChance = 0;
    uint32 durationIndex = 0;
    uint32 effect = 0;
    uint32 effectImplicitTargetA = 0;
    uint32 effectImplicitTargetB = 0;
    uint32 effectRadiusIndex = 0;
    uint32 effectApplyAuraName = 0;
    uint32 effectMiscValue = 0;
    uint32 effectMiscValueB = 0;
    uint32 effectTriggerSpell = 0;
    std::string comments;
};

/// The server-side spells on the world database.
namespace SpellTemplateRepository
{
    /// Reads one row: id, the four attribute columns, proc flags and chance, duration index, the
    /// eight effect columns and the comment (NULL as "").
    SpellTemplateRow ReadRow(Field* fields);

    /// Reads every row, synchronously, in the order the database returns them, with how the read ended.
    TableRead<SpellTemplateRow> QuerySpellTemplates();
}

#endif
