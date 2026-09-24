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

#include "combat/SpellBonus.h"

#include "SharedDefines.h"

#include <algorithm>

namespace Combat
{
    uint32 SpellCriticalHealingBonus(uint32 damage, float criticalHealingMultiplier)
    {
        // Calculate critical bonus
        int32 crit_bonus = damage;

        if (crit_bonus > 0)
        {
            damage += crit_bonus;
        }

        damage = int32(damage * criticalHealingMultiplier);

        return damage;
    }

    int32 SpellCritDamageBonusBase(uint32 damage, uint32 dmgClass, int32 critDamageBonusPct)
    {
        // Calculate critical bonus
        int32 crit_bonus;
        switch (dmgClass)
        {
            case SPELL_DAMAGE_CLASS_MELEE:                      // for melee based spells is 100%
            case SPELL_DAMAGE_CLASS_RANGED:
                crit_bonus = damage;
                break;
            default:
                crit_bonus = damage / 2;                        // for spells is 50%
                break;
        }

        // Apply SPELL_AURA_MOD_CRIT_DAMAGE_BONUS modifier first
        crit_bonus += int32((damage + crit_bonus) * float(critDamageBonusPct / 100.0f));

        return crit_bonus;
    }

    uint32 SpellCritDamageBonusTaken(uint32 damage, int32 crit_bonus, bool hasVictim,
                                     uint32 dmgClass, bool isRangedAttack,
                                     int32 victimRangedCritDamageMod,
                                     int32 victimMeleeCritDamageMod,
                                     int32 victimSpellCritDamageMod)
    {
        if (!hasVictim)
        {
            return damage += crit_bonus;
        }

        int32 critPctDamageMod = 0;
        if (dmgClass >= SPELL_DAMAGE_CLASS_MELEE)
        {
            if (isRangedAttack)
            {
                critPctDamageMod += victimRangedCritDamageMod;
            }
            else
            {
                critPctDamageMod += victimMeleeCritDamageMod;
            }
        }
        else
        {
            critPctDamageMod += victimSpellCritDamageMod;
        }

        if (critPctDamageMod != 0)
        {
            crit_bonus = int32(crit_bonus * float((100.0f + critPctDamageMod) / 100.0f));
        }

        if (crit_bonus > 0)
        {
            damage += crit_bonus;
        }

        return damage;
    }

    float SpellDamageTakenPercent(float TakenTotalMod, float mechanicDamageTakenMultiplier,
                                  bool isAreaOfEffectSpell, float aoeDamageAvoidanceMultiplier,
                                  bool isPet, float petAoeDamageAvoidanceMultiplier)
    {
        // Mod damage from spell mechanic
        TakenTotalMod *= mechanicDamageTakenMultiplier;

        // Mod damage taken from AoE spells
        if (isAreaOfEffectSpell)
        {
            TakenTotalMod *= aoeDamageAvoidanceMultiplier;
            if (isPet)
            {
                TakenTotalMod *= petAoeDamageAvoidanceMultiplier;
            }
        }

        return TakenTotalMod;
    }

    float SpellHealingTakenPercent(int32 healingPctNegative, int32 healingPctPositive)
    {
        float  TakenTotalMod = 1.0f;

        // Healing taken percent
        float minval = float(healingPctNegative);
        if (minval)
        {
            TakenTotalMod *= (100.0f + minval) / 100.0f;
        }

        float maxval = float(healingPctPositive);
        // no SPELL_AURA_MOD_PERIODIC_HEAL positive cases
        if (maxval)
        {
            TakenTotalMod *= (100.0f + maxval) / 100.0f;
        }

        return TakenTotalMod;
    }

    SpellLegacyScaling SpellLegacyScalingPoints(uint32 level, uint32 spellLevel, uint32 maxLevel,
                                                uint32 baseLevel, float basePointsPerLevel,
                                                bool hasEffBasePoints, int32 effBasePoints,
                                                int32 effectBasePoints, int32 effectDieSides,
                                                float effectPointsPerResource)
    {
        if (maxLevel)
        {
            level = std::min(level, maxLevel);
        }
        level = std::max(level, baseLevel);
        level = std::max(level, spellLevel) - spellLevel;

        int32 basePoints = hasEffBasePoints ? effBasePoints - 1 : effectBasePoints;
        basePoints += int32(level * basePointsPerLevel);
        int32 randomPoints = int32(effectDieSides);
        float comboDamage = effectPointsPerResource;

        SpellLegacyScaling result;
        result.level = level;
        result.basePoints = basePoints;
        result.randomPoints = randomPoints;
        result.comboDamage = comboDamage;
        return result;
    }
}
