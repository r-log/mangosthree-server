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
}
