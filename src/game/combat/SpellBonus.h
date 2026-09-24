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

#ifndef MANGOSSERVER_COMBAT_SPELLBONUS_H
#define MANGOSSERVER_COMBAT_SPELLBONUS_H

#include "Platform/Define.h"

namespace Combat
{
    /**
     * @brief Applies critical healing bonuses for a spell heal.
     *
     * Unit::SpellCriticalHealingBonus, src/game/Object/UnitSpellBonus.cpp:1060-1073 at
     * ce20c27db, moved verbatim. The member's `spellProto` and `pVictim` parameters are
     * never read by the body, so they have no counterpart here.
     *
     * @param damage The base healing amount. The body doubles it and then truncates the
     *        product to int32, so a caller above INT32_MAX/2 is outside the range where
     *        that conversion is defined -- as it already was on Unit.
     * @param criticalHealingMultiplier GetTotalAuraMultiplier(SPELL_AURA_MOD_CRITICAL_HEALING_AMOUNT).
     * @return The healing after critical bonuses.
     */
    uint32 SpellCriticalHealingBonus(uint32 damage, float criticalHealingMultiplier);
}

#endif // MANGOSSERVER_COMBAT_SPELLBONUS_H
