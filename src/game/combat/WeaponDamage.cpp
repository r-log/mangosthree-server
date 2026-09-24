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

#include "combat/WeaponDamage.h"

#include <utility>                                              // std::swap

namespace Combat
{
    void CalculateMinMaxDamage(WeaponAttackType attType, float attackSpeedMultiplier,
                               float modifierBaseValue, float modifierBasePct,
                               float modifierTotalValue, float modifierTotalPct,
                               float totalAttackPower, float weaponMinDamage,
                               float weaponMaxDamage, bool isInFeralForm,
                               ShapeshiftForm shapeshiftForm, bool canUseEquippedWeapon,
                               uint32 attackTime, float ammoDPS, float baseMinDamage,
                               float baseMaxDamage, float& min_damage, float& max_damage)
    {
        float att_speed = attackSpeedMultiplier;

        float base_value  = modifierBaseValue + totalAttackPower / 14.0f * att_speed;
        float base_pct    = modifierBasePct;
        float total_value = modifierTotalValue;
        float total_pct   = modifierTotalPct;

        float weapon_mindamage = weaponMinDamage;
        float weapon_maxdamage = weaponMaxDamage;

        if (isInFeralForm)                                      // check if player is druid and in cat or bear forms, non main hand attacks not allowed for this mode so not check attack type
        {
            float weaponSpeed = attackTime / 1000.0f;

            switch (shapeshiftForm)
            {
                case FORM_CAT:
                    weapon_mindamage = weapon_mindamage / weaponSpeed;
                    weapon_maxdamage = weapon_maxdamage / weaponSpeed;
                    break;
                case FORM_BEAR:
                    weapon_mindamage = weapon_mindamage / weaponSpeed + weapon_mindamage / 2.5f;
                    weapon_maxdamage = weapon_maxdamage / weaponSpeed + weapon_maxdamage / 2.5f;
                    break;
            }
        }
        else if (!canUseEquippedWeapon)                         // check if player not in form but still can't use weapon (broken/etc)
        {
            weapon_mindamage = baseMinDamage;
            weapon_maxdamage = baseMaxDamage;
        }
        else if (attType == RANGED_ATTACK)                      // add ammo DPS to ranged damage
        {
            weapon_mindamage += ammoDPS * att_speed;
            weapon_maxdamage += ammoDPS * att_speed;
        }

        min_damage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
        max_damage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;
    }

    MeleeDamageTakenParts MeleeDamageTaken(WeaponAttackType attType, int32 rangedDamageTakenMod,
                                           int32 meleeDamageTakenMod, int32 damageTakenSchoolMod,
                                           float damagePercentTakenMultiplier,
                                           float mechanicDamageTakenMultiplier,
                                           float rangedDamageTakenPct, float meleeDamageTakenPct,
                                           bool isAreaOfEffectSpell,
                                           float aoeDamageAvoidanceMultiplier, bool isPet,
                                           float petAoeDamageAvoidanceMultiplier)
    {
        // FLAT damage bonus auras
        // =======================
        int32 TakenFlat = 0;

        // ..taken flat (base at attack power for marked target and base at attack power for creature type)
        if (attType == RANGED_ATTACK)
        {
            TakenFlat += rangedDamageTakenMod;
        }
        else
        {
            TakenFlat += meleeDamageTakenMod;
        }

        // ..taken flat (by school mask)
        TakenFlat += damageTakenSchoolMod;

        // PERCENT damage auras
        // ====================
        float TakenPercent  = 1.0f;

        // ..taken pct (by school mask)
        TakenPercent *= damagePercentTakenMultiplier;

        // ..taken pct (by mechanic mask)
        TakenPercent *= mechanicDamageTakenMultiplier;

        // ..taken pct (melee/ranged)
        if (attType == RANGED_ATTACK)
        {
            TakenPercent *= rangedDamageTakenPct;
        }
        else
        {
            TakenPercent *= meleeDamageTakenPct;
        }

        // ..taken pct (aoe avoidance)
        if (isAreaOfEffectSpell)
        {
            TakenPercent *= aoeDamageAvoidanceMultiplier;
            if (isPet)
            {
                TakenPercent *= petAoeDamageAvoidanceMultiplier;
            }
        }

        MeleeDamageTakenParts result;
        result.TakenFlat = TakenFlat;
        result.TakenPercent = TakenPercent;
        return result;
    }

    MeleeDamageDoneParts MeleeDamageDoneBase(int32 DoneFlat, int32 APbonus, WeaponAttackType attType,
                                             int32 damageDoneCreatureMod,
                                             int32 victimRangedApAttackerBonus,
                                             int32 rangedApVersusMod,
                                             int32 victimMeleeApAttackerBonus,
                                             int32 meleeApVersusMod)
    {
        // ..done flat (by creature type mask)
        DoneFlat += damageDoneCreatureMod;

        // ..done flat (base at attack power for marked target and base at attack power for creature type)
        if (attType == RANGED_ATTACK)
        {
            APbonus += victimRangedApAttackerBonus;
            APbonus += rangedApVersusMod;
        }
        else
        {
            APbonus += victimMeleeApAttackerBonus;
            APbonus += meleeApVersusMod;
        }

        // PERCENT damage auras
        // ====================
        float DonePercent   = 1.0f;

        MeleeDamageDoneParts result;
        result.DoneFlat = DoneFlat;
        result.APbonus = APbonus;
        result.DonePercent = DonePercent;
        return result;
    }

    float MeleeDamageDoneWeaponBased(float DoneTotal, int32 APbonus, int32 DoneFlat,
                                     float apMultiplier, float damageTotalPct)
    {
        DoneTotal += int32(APbonus / 14.0f * apMultiplier);

        // for weapon damage based spells we still have to apply damage done percent mods
        // (that are already included into pdamage) to not-yet included DoneFlat
        // e.g. from doneVersusCreature, apBonusVs...
        DoneTotal += DoneFlat;

        DoneTotal *= damageTotalPct;

        return DoneTotal;
    }

    WeaponDamageRange SelectWeaponDamageRange(WeaponAttackType attType, bool isNormalizedPlayer,
                                              float playerMinDamage, float playerMaxDamage,
                                              float minRangedDamage, float maxRangedDamage,
                                              float minBaseDamage, float maxBaseDamage,
                                              float minOffhandDamage, float maxOffhandDamage)
    {
        float min_damage, max_damage;

        if (isNormalizedPlayer)
        {
            min_damage = playerMinDamage;
            max_damage = playerMaxDamage;
        }
        else
        {
            switch (attType)
            {
                case RANGED_ATTACK:
                    min_damage = minRangedDamage;
                    max_damage = maxRangedDamage;
                    break;
                case BASE_ATTACK:
                    min_damage = minBaseDamage;
                    max_damage = maxBaseDamage;
                    break;
                case OFF_ATTACK:
                    min_damage = minOffhandDamage;
                    max_damage = maxOffhandDamage;
                    break;
                    // Just for good manner
                default:
                    min_damage = 0.0f;
                    max_damage = 0.0f;
                    break;
            }
        }

        if (min_damage > max_damage)
        {
            std::swap(min_damage, max_damage);
        }

        if (max_damage == 0.0f)
        {
            max_damage = 5.0f;
        }

        WeaponDamageRange result;
        result.min_damage = min_damage;
        result.max_damage = max_damage;
        return result;
    }
}
