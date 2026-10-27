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

#include "SpellTemplateRepository.h"
#include "Database/DatabaseEnv.h"
#include "persistence/RowReader.h"

namespace SpellTemplateRepository
{
    SpellTemplateRow ReadRow(Field* fields)
    {
        SpellTemplateRow row;
        row.id = fields[0].GetUInt32();
        row.attributes = fields[1].GetUInt32();
        row.attributesEx = fields[2].GetUInt32();
        row.attributesEx2 = fields[3].GetUInt32();
        row.attributesEx3 = fields[4].GetUInt32();
        row.procFlags = fields[5].GetUInt32();
        row.procChance = fields[6].GetUInt32();
        row.durationIndex = fields[7].GetUInt32();
        row.effect = fields[8].GetUInt32();
        row.effectImplicitTargetA = fields[9].GetUInt32();
        row.effectImplicitTargetB = fields[10].GetUInt32();
        row.effectRadiusIndex = fields[11].GetUInt32();
        row.effectApplyAuraName = fields[12].GetUInt32();
        row.effectMiscValue = fields[13].GetUInt32();
        row.effectMiscValueB = fields[14].GetUInt32();
        row.effectTriggerSpell = fields[15].GetUInt32();
        char const* comments = fields[16].GetString();
        row.comments = comments ? comments : "";
        return row;
    }

    TableRead<SpellTemplateRow> QuerySpellTemplates()
    {
        //                                               0     1       2          3           4           5             6              7                 8          9                            10                           11                    12                         13                    14                      15                       16
        return ReadWorldTable<SpellTemplateRow>("SELECT `id`, `attr`, `attr_ex`, `attr_ex2`, `attr_ex3`, `proc_flags`, `proc_chance`, `duration_index`, `effect0`, `effect0_implicit_target_a`, `effect0_implicit_target_b`, `effect0_radius_idx`, `effect0_apply_aura_name`, `effect0_misc_value`, `effect0_misc_value_b`, `effect0_trigger_spell`, `comments` FROM `spell_template`", ReadRow);
    }
}
