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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_CIRCLEOFHEALING_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_CIRCLEOFHEALING_H

#include "spells/classes/priest/Priest.h"

#include <list>

class Unit;

/**
 * @file CircleOfHealing.h
 * @brief Circle of Healing, the Holy talent (34861): the units its heal picks, the members of the priest's raid or
 * party and their pets, the most injured first: the leaves of CircleOfHealing.cpp.
 *
 * CircleOfHealing.cpp registers two rows, at the select-targets hook of 34861, the priest's Circle of Healing, and of
 * 49306, an older rank a creature casts at itself. The heal's effect fills its units in two target modes: the first
 * (TARGET_DUELVSPLAYER_COORDINATES) puts the cast's destination at its unit target, the second
 * (TARGET_ALL_FRIENDLY_UNITS_IN_AREA) takes every friendly unit within the effect's yards of that destination, the
 * engine's area fill. At the second, the row keeps of them the units the heal may take (CircleOfHealingTakes): the
 * members of the caster's raid or party and the pet of each, him and his own pet included, and of those the living
 * ones of the lowest health percent (CircleOfHealingMostInjured), as many as CircleOfHealingUnits names: 5, and one
 * more for each point of the effect 0 amount of his Glyph of Circle of Healing (55675, 1 in the client's data;
 * CircleOfHealingGlyph).
 */

namespace Priest
{
    /// Registers Circle of Healing's rows on `registry`: the select-targets row of 34861 and of 49306; answers 2, the
    /// rows registered.
    uint32 RegisterCircleOfHealing(SpellHandlerRegistry& registry);

    /// The most units a Circle of Healing heals for a glyph amount of `glyph`: the 5 of the client's text of 34861
    /// ("Heals up to $?s55675[6][5] friendly party or raid members"), which no effect holds, plus `glyph`; a glyph
    /// amount of 0 or less adds nothing.
    uint32 CircleOfHealingUnits(int32 glyph);

    /// The effect 0 amount of the Glyph of Circle of Healing (55675) `caster` holds; 0 when he holds none.
    int32 CircleOfHealingGlyph(Unit const& caster);

    /// Whether a Circle of Healing `caster` casts may heal `unit`, as the engine's raid and party target fill takes
    /// its units with pets (Spell::FillRaidOrPartyTargets): with a player behind the caster (himself, his charmer or
    /// his owner), a player that is that player or a member of his group, and the pet of such a player (its owner's
    /// current pet), the player not hostile to the caster; with none, the caster's charmer or owner, or the caster
    /// itself without one, and that unit's current pet. Membership is the raid buffs' rule (CastsRaidWideBuff): the
    /// whole raid, every subgroup.
    bool CircleOfHealingTakes(Unit const& caster, Unit const& unit);

    /// The health percent Circle of Healing ranks `unit` by: its health times 100 over its maximum health, truncated,
    /// as the engine's health-priority fill ranks it; 0 for a maximum health of 0.
    uint32 CircleOfHealingHealthPercent(Unit const& unit);

    /// Keeps of `units` the `count` living ones of the lowest health percent (CircleOfHealingHealthPercent), in that
    /// order; units of one percent keep their order in the list. A dead unit, or none, is dropped; `count` 0 leaves
    /// none.
    void CircleOfHealingMostInjured(std::list<Unit*>& units, uint32 count);
}

#endif
