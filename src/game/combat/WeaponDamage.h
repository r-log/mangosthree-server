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

#ifndef MANGOSSERVER_COMBAT_WEAPONDAMAGE_H
#define MANGOSSERVER_COMBAT_WEAPONDAMAGE_H

#include "Platform/Define.h"
#include "SharedDefines.h"

namespace Combat
{
    /**
     * @brief Calculates the player's minimum and maximum weapon damage.
     *
     * Player::CalculateMinMaxDamage, src/game/Object/StatSystem.cpp:448-509 at ce20c27db,
     * moved verbatim. The member's opening switch only chooses WHICH UnitMods the four
     * GetModifierValue reads use, so it stays on Player and the four values arrive here
     * already read; `normalized` likewise only feeds GetAPMultiplier, whose result is
     * attackSpeedMultiplier.
     *
     * @param attType The attack type to evaluate.
     * @param attackSpeedMultiplier GetAPMultiplier(attType, normalized).
     * @param modifierBaseValue GetModifierValue(unitMod, BASE_VALUE).
     * @param modifierBasePct GetModifierValue(unitMod, BASE_PCT).
     * @param modifierTotalValue GetModifierValue(unitMod, TOTAL_VALUE).
     * @param modifierTotalPct GetModifierValue(unitMod, TOTAL_PCT).
     * @param totalAttackPower GetTotalAttackPowerValue(attType).
     * @param weaponMinDamage GetWeaponDamageRange(attType, MINDAMAGE).
     * @param weaponMaxDamage GetWeaponDamageRange(attType, MAXDAMAGE).
     * @param isInFeralForm IsInFeralForm().
     * @param shapeshiftForm GetShapeshiftForm(), FORM_NONE outside a feral form.
     * @param canUseEquippedWeapon CanUseEquippedWeapon(attType), false where the feral arm wins.
     * @param attackTime GetAttackTime(attType), 0 outside the feral arm that reads it.
     * @param ammoDPS GetAmmoDPS(), 0 outside the ranged arm that reads it.
     * @param baseMinDamage BASE_MINDAMAGE (Unit.h:219), passed so the constant keeps one definition.
     * @param baseMaxDamage BASE_MAXDAMAGE (Unit.h:220), likewise.
     * @param min_damage Receives the minimum damage value.
     * @param max_damage Receives the maximum damage value.
     */
    void CalculateMinMaxDamage(WeaponAttackType attType, float attackSpeedMultiplier,
                               float modifierBaseValue, float modifierBasePct,
                               float modifierTotalValue, float modifierTotalPct,
                               float totalAttackPower, float weaponMinDamage,
                               float weaponMaxDamage, bool isInFeralForm,
                               ShapeshiftForm shapeshiftForm, bool canUseEquippedWeapon,
                               uint32 attackTime, float ammoDPS, float baseMinDamage,
                               float baseMaxDamage, float& min_damage, float& max_damage);
}

#endif // MANGOSSERVER_COMBAT_WEAPONDAMAGE_H
