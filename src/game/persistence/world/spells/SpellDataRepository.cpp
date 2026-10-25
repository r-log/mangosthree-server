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

#include "SpellDataRepository.h"
#include "Database/DatabaseEnv.h"

namespace SpellDataRepository
{
    QueryResult* QueryUsedSpells(char const* table)
    {
        //                                   0         1                 2                  3                  4           5             6               7            8            9           10     11
        return WorldDatabase.PQuery("SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMaskA`,`SpellFamilyMaskB`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `%s`", table);
    }

    QueryResult* QuerySpellAreas()
    {
        //                                 0      1     2            3                   4          5             6           7         8       9
        return WorldDatabase.Query("SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`");
    }

    QueryResult* QuerySpellChains()
    {
        return WorldDatabase.Query("SELECT `spell_id`, `prev_spell`, `first_spell`, `rank`, `req_spell` FROM `spell_chain`");
    }

    QueryResult* QueryLearnSpells()
    {
        //                                 0      1        2
        return WorldDatabase.Query("SELECT `entry`, `SpellID`, `Active` FROM `spell_learn_spell`");
    }

    QueryResult* QueryTargetPositions()
    {
        //                                 0   1           2                  3                  4                  5
        return WorldDatabase.Query("SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`");
    }

    QueryResult* QueryBonuses()
    {
        return WorldDatabase.Query("SELECT entry, direct_bonus,         `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`");
    }

    QueryResult* QueryElixirs()
    {
        //                                 0      1
        return WorldDatabase.Query("SELECT `entry`, `mask` FROM `spell_elixir`");
    }

    QueryResult* QueryThreats()
    {
        //                                 0      1       2           3
        return WorldDatabase.Query("SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`");
    }

    QueryResult* QueryPetAuras()
    {
        //                                  0        1           2      3
        return WorldDatabase.Query("SELECT `spell`, `effectId`, `pet`, `aura` FROM `spell_pet_auras`");
    }

    QueryResult* QueryProcEvents()
    {
        //                                  0        1             2                  3                    4                    5                    6                    7                    8                    9                    10                   11                   12           13        14         15              16
        return WorldDatabase.Query("SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMaskA0`, `SpellFamilyMaskA1`, `SpellFamilyMaskA2`, `SpellFamilyMaskB0`, `SpellFamilyMaskB1`, `SpellFamilyMaskB2`, `SpellFamilyMaskC0`, `SpellFamilyMaskC1`, `SpellFamilyMaskC2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`");
    }

    QueryResult* QueryProcItemEnchants()
    {
        //                                 0      1
        return WorldDatabase.Query("SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`");
    }
}
