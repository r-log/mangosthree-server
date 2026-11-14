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

/**
 * @file DeathPrevention.h
 * @brief How a death-prevention aura saves its bearer from a blow that would kill him: which of his auras saves,
 * whether the blow kills, how much of it the bearer still takes and how much he is healed. Pure functions over numbers;
 * the shapes are the death prevention table's column (spells/data/SpellTables.h).
 */

#ifndef MANGOSSERVER_COMBAT_DEATHPREVENTION_H
#define MANGOSSERVER_COMBAT_DEATHPREVENTION_H

#include "Platform/Define.h"
#include "spells/data/SpellTables.h"

#include <cstddef>
#include <vector>

namespace Combat
{
    /**
     * @brief Which of several death-prevention auras that meet one blow saves.
     *
     * @param usedUp For each aura, in the order the unit holds them, whether its save uses it up.
     * @return The index of the first aura its save uses up, else 0: the first.
     */
    std::size_t DeathPreventionChoice(std::vector<bool> const& usedUp);

    /// What a save leaves of the blow and gives back.
    struct DeathPreventionOutcome
    {
        int32 damageKept;   ///< the part of the blow the bearer still takes
        int32 heal;         ///< the health the save gives back; 0 for none
    };

    /**
     * @brief Whether a blow kills its target.
     *
     * @param remainingDamage The blow's damage after the resists, the shields and the split damage.
     * @param health The target's health.
     * @return True when the damage is positive and at least the health.
     */
    bool WouldKill(int32 remainingDamage, uint32 health);

    /**
     * @brief The save from a killing blow. The floor is `percent` of `maxHealth`, truncated.
     *
     * @param shape How the aura saves.
     * @param health The bearer's health before the blow.
     * @param maxHealth The bearer's maximum health.
     * @param percent The health percent the aura names.
     * @return DEATH_PREVENTION_FLOOR: the damage kept is `health - floor`, 0 at or below the floor, and no heal;
     *         DEATH_PREVENTION_FLOOR_AND_HEAL: the same damage kept above the floor, and below it a heal of
     *         `floor - health`; DEATH_PREVENTION_HEAL_PERCENT: no damage kept and a heal of the floor.
     */
    DeathPreventionOutcome SaveFromKillingBlow(DeathPreventionShape shape, uint32 health, uint32 maxHealth, uint32 percent);
}

#endif
