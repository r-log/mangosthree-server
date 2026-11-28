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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_IMPROVEDDEVOURINGPLAGUE_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_IMPROVEDDEVOURINGPLAGUE_H

#include "spells/classes/priest/Priest.h"

/**
 * @file ImprovedDevouringPlague.h
 * @brief Improved Devouring Plague: the instant hit a Devouring Plague gives: the leaves of
 * ImprovedDevouringPlague.cpp.
 *
 * Improved Devouring Plague (63625, 63626) has the text "Your Devouring Plague instantly deals damage equal to $s1% of
 * its total periodic effect", $s1 the rank's effect 0 (15 or 30). ImprovedDevouringPlague.cpp registers an on-proc row
 * on each rank: at a hit of the priest's Devouring Plague (2944) the priest casts the talent's hit (63675) at the target,
 * triggered by the rank's aura, for that percent (ImprovedDevouringPlagueHit) of the whole periodic effect the plague
 * the target holds will deal: the tick it holds, which already has the priest's done bonus, times the ticks it has to
 * run (DevouringPlagueTicks, its time left over its period at the hit, which his haste shortens, rounded up). The hit
 * itself takes no done bonus (Spells::DamageTakesNoDoneBonus), so the priest's bonus counts once.
 */

namespace Priest
{
    /// Registers the on-proc row of each rank of Improved Devouring Plague on `registry`; answers 2, the rows registered.
    uint32 RegisterImprovedDevouringPlague(SpellHandlerRegistry& registry);

    /// The ticks a periodic aura with `remainingMs` left deals at one tick every `periodMs`: the time left over the
    /// period, rounded up. A plague made at the hit has a whole number of periods left, and one that replaced another
    /// has the time to the old one's next tick as well (UnitAura.cpp CarryPeriodicTimers), which makes a tick of its own
    /// at that time; the engine counts the same (Spells::TickCount). 0 when either is not above 0 (an aura that does not
    /// run out has no time to count).
    int32 DevouringPlagueTicks(int32 remainingMs, int32 periodMs);

    /// The instant hit of a plague whose tick is `tick` and that has `ticks` ticks to run, for a rank of `percent`
    /// percent: `percent` percent of `tick` times `ticks`, truncated; 0 when any of the three is not above 0, and no
    /// more than the largest amount a spell is cast with.
    int32 ImprovedDevouringPlagueHit(int32 tick, int32 ticks, int32 percent);
}

#endif
