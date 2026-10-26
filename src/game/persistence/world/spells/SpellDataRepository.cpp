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
#include "persistence/RowReader.h"

namespace SpellDataRepository
{
    static UsedSpellRow ReadUsedSpell(Field* fields)
    {
        UsedSpellRow row;
        row.spellid = fields[0].GetUInt32();
        row.spellFamilyName = fields[1].GetInt32();
        row.spellFamilyMaskA = fields[2].GetUInt64();
        row.spellFamilyMaskB = fields[3].GetUInt32();
        row.spellIcon = fields[4].GetInt32();
        row.spellVisual = fields[5].GetInt32();
        row.spellCategory = fields[6].GetInt32();
        row.effectType = fields[7].GetInt32();
        row.effectAura = fields[8].GetInt32();
        row.effectIdx = fields[9].GetInt32();
        row.name = fields[10].GetCppString();
        row.code = fields[11].GetCppString();
        return row;
    }

    std::vector<UsedSpellRow> QueryUsedSpells(char const* table)
    {
        //                                   0         1                 2                  3                  4           5             6               7            8            9           10     11
        QueryResult* result = WorldDatabase.PQuery("SELECT `spellid`,`SpellFamilyName`,`SpellFamilyMaskA`,`SpellFamilyMaskB`,`SpellIcon`,`SpellVisual`,`SpellCategory`,`EffectType`,`EffectAura`,`EffectIdx`,`Name`,`Code` FROM `%s`", table);
        return ReadRows<UsedSpellRow>(result, ReadUsedSpell);
    }

    static SpellAreaRow ReadSpellArea(Field* fields)
    {
        SpellAreaRow row;
        row.spell = fields[0].GetUInt32();
        row.area = fields[1].GetUInt32();
        row.questStart = fields[2].GetUInt32();
        row.questStartActive = fields[3].GetBool();
        row.questEnd = fields[4].GetUInt32();
        row.conditionId = fields[5].GetUInt16();
        row.auraSpell = fields[6].GetInt32();
        row.racemask = fields[7].GetUInt32();
        row.gender = fields[8].GetUInt8();
        row.autocast = fields[9].GetBool();
        return row;
    }

    std::vector<SpellAreaRow> QuerySpellAreas()
    {
        //                                 0      1     2            3                   4          5             6           7         8       9
        QueryResult* result = WorldDatabase.Query("SELECT `spell`, `area`, `quest_start`, `quest_start_active`, `quest_end`, `condition_id`, `aura_spell`, `racemask`, `gender`, `autocast` FROM `spell_area`");
        return ReadRows<SpellAreaRow>(result, ReadSpellArea);
    }

    static SpellChainRow ReadSpellChain(Field* fields)
    {
        SpellChainRow row;
        row.spellId = fields[0].GetUInt32();
        row.prevSpell = fields[1].GetUInt32();
        row.firstSpell = fields[2].GetUInt32();
        row.rank = fields[3].GetUInt8();
        row.reqSpell = fields[4].GetUInt32();
        return row;
    }

    std::vector<SpellChainRow> QuerySpellChains()
    {
        QueryResult* result = WorldDatabase.Query("SELECT `spell_id`, `prev_spell`, `first_spell`, `rank`, `req_spell` FROM `spell_chain`");
        return ReadRows<SpellChainRow>(result, ReadSpellChain);
    }

    static LearnSpellRow ReadLearnSpell(Field* fields)
    {
        LearnSpellRow row;
        row.entry = fields[0].GetUInt32();
        row.spellID = fields[1].GetUInt32();
        row.active = fields[2].GetBool();
        return row;
    }

    std::vector<LearnSpellRow> QueryLearnSpells()
    {
        //                                 0      1        2
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `SpellID`, `Active` FROM `spell_learn_spell`");
        return ReadRows<LearnSpellRow>(result, ReadLearnSpell);
    }

    static SpellTargetPositionRow ReadSpellTargetPosition(Field* fields)
    {
        SpellTargetPositionRow row;
        row.id = fields[0].GetUInt32();
        row.targetMap = fields[1].GetUInt32();
        row.targetPositionX = fields[2].GetFloat();
        row.targetPositionY = fields[3].GetFloat();
        row.targetPositionZ = fields[4].GetFloat();
        row.targetOrientation = fields[5].GetFloat();
        return row;
    }

    std::vector<SpellTargetPositionRow> QueryTargetPositions()
    {
        //                                 0   1           2                  3                  4                  5
        QueryResult* result = WorldDatabase.Query("SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`");
        return ReadRows<SpellTargetPositionRow>(result, ReadSpellTargetPosition);
    }

    static SpellBonusRow ReadSpellBonus(Field* fields)
    {
        SpellBonusRow row;
        row.entry = fields[0].GetUInt32();
        row.directBonus = fields[1].GetFloat();
        row.dotBonus = fields[2].GetFloat();
        row.apBonus = fields[3].GetFloat();
        row.apDotBonus = fields[4].GetFloat();
        return row;
    }

    std::vector<SpellBonusRow> QueryBonuses()
    {
        QueryResult* result = WorldDatabase.Query("SELECT entry, direct_bonus,         `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`");
        return ReadRows<SpellBonusRow>(result, ReadSpellBonus);
    }

    static SpellElixirRow ReadSpellElixir(Field* fields)
    {
        SpellElixirRow row;
        row.entry = fields[0].GetUInt32();
        row.mask = fields[1].GetUInt8();
        return row;
    }

    std::vector<SpellElixirRow> QueryElixirs()
    {
        //                                 0      1
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `mask` FROM `spell_elixir`");
        return ReadRows<SpellElixirRow>(result, ReadSpellElixir);
    }

    static SpellThreatRow ReadSpellThreat(Field* fields)
    {
        SpellThreatRow row;
        row.entry = fields[0].GetUInt32();
        row.threat = fields[1].GetUInt16();
        row.multiplier = fields[2].GetFloat();
        row.apBonus = fields[3].GetFloat();
        return row;
    }

    std::vector<SpellThreatRow> QueryThreats()
    {
        //                                 0      1       2           3
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`");
        return ReadRows<SpellThreatRow>(result, ReadSpellThreat);
    }

    static SpellPetAuraRow ReadSpellPetAura(Field* fields)
    {
        SpellPetAuraRow row;
        row.spell = fields[0].GetUInt32();
        row.effectId = fields[1].GetUInt32();
        row.pet = fields[2].GetUInt32();
        row.aura = fields[3].GetUInt32();
        return row;
    }

    std::vector<SpellPetAuraRow> QueryPetAuras()
    {
        //                                  0        1           2      3
        QueryResult* result = WorldDatabase.Query("SELECT `spell`, `effectId`, `pet`, `aura` FROM `spell_pet_auras`");
        return ReadRows<SpellPetAuraRow>(result, ReadSpellPetAura);
    }

    static SpellProcEventRow ReadSpellProcEvent(Field* fields)
    {
        SpellProcEventRow row;
        row.entry = fields[0].GetUInt32();
        row.schoolMask = fields[1].GetUInt32();
        row.spellFamilyName = fields[2].GetUInt32();
        for (int i = 0; i < SPELL_PROC_EVENT_ROW_MASKS; ++i)
        {
            row.spellFamilyMaskA[i] = fields[i + 3].GetUInt32();
            row.spellFamilyMaskB[i] = fields[i + 6].GetUInt32();
            row.spellFamilyMaskC[i] = fields[i + 9].GetUInt32();
        }
        row.procFlags = fields[12].GetUInt32();
        row.procEx = fields[13].GetUInt32();
        row.ppmRate = fields[14].GetFloat();
        row.customChance = fields[15].GetFloat();
        row.cooldown = fields[16].GetUInt32();
        return row;
    }

    std::vector<SpellProcEventRow> QueryProcEvents()
    {
        //                                  0        1             2                  3                    4                    5                    6                    7                    8                    9                    10                   11                   12           13        14         15              16
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMaskA0`, `SpellFamilyMaskA1`, `SpellFamilyMaskA2`, `SpellFamilyMaskB0`, `SpellFamilyMaskB1`, `SpellFamilyMaskB2`, `SpellFamilyMaskC0`, `SpellFamilyMaskC1`, `SpellFamilyMaskC2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`");
        return ReadRows<SpellProcEventRow>(result, ReadSpellProcEvent);
    }

    static SpellProcItemEnchantRow ReadSpellProcItemEnchant(Field* fields)
    {
        SpellProcItemEnchantRow row;
        row.entry = fields[0].GetUInt32();
        row.ppmRate = fields[1].GetFloat();
        return row;
    }

    std::vector<SpellProcItemEnchantRow> QueryProcItemEnchants()
    {
        //                                 0      1
        QueryResult* result = WorldDatabase.Query("SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`");
        return ReadRows<SpellProcItemEnchantRow>(result, ReadSpellProcItemEnchant);
    }
}
