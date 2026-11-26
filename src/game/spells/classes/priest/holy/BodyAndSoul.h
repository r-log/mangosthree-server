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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_BODYANDSOUL_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_BODYANDSOUL_H

#include "spells/classes/priest/Priest.h"

#include <functional>

class Unit;

/**
 * @file BodyAndSoul.h
 * @brief Body and Soul, the Holy talent (64127, 64129): the speed it gives the target of the priest's Power Word:
 * Shield and Leap of Faith, and the poison it takes off him with his Cure Disease: the leaves of BodyAndSoul.cpp.
 *
 * BodyAndSoul.cpp registers three rows. Power Word: Shield (17), once its holder is added to a unit: the priest who
 * cast it casts his rank's speed (BodyAndSoulSpeed: 64128, +30%, or 65081, +60%, for 4 seconds) at that unit. Leap of
 * Faith (73325), once his cast of it is over: he casts the same at the unit it pulled. Its dummy effect is
 * LeapOfFaith.cpp's row, and its hit deals no heal and no damage, so no after-hit row would run for it. Cure Disease
 * (528), once his cast of it at himself is over: at his rank's chance (BodyAndSoulPoisonChance: effect 1 of the rank,
 * 50 or 100 in the client's data) he casts 64136 at himself, the client's Body and Soul dispel, whose effect takes
 * one poison off him through the engine's dispel of its type. The talent's effect masks name Power Word: Shield alone,
 * so the script keys on the three spells' ids.
 */

namespace Priest
{
    /// Registers Body and Soul's rows on `registry`: the aura-apply row of Power Word: Shield (17) and the after-cast
    /// rows of Leap of Faith (73325) and Cure Disease (528); answers 3, the rows registered.
    uint32 RegisterBodyAndSoul(SpellHandlerRegistry& registry);

    /// The rank of Body and Soul `priest` holds: 64129 when he holds the second rank, else 64127 when he holds the
    /// first; 0 when he holds neither.
    uint32 BodyAndSoulRank(Unit const& priest);

    /// The speed spell of the rank `rank`: 64128 for 64127, 65081 for 64129; 0 for any other spell.
    uint32 BodyAndSoulSpeed(uint32 rank);

    /// The chance in percent that the rank `rank` takes a poison off the priest with his Cure Disease: the rank's
    /// effect 1 value in the client's data (50 for 64127, 100 for 64129); 0 for any other spell, or when the store
    /// lacks the rank.
    int32 BodyAndSoulPoisonChance(uint32 rank);

    /// Whether a chance of `chance` percent takes the poison off: never at 0 or less, always at 100 or more, and in
    /// between as `roll(chance)` answers, the only case that draws a roll.
    bool BodyAndSoulCleansesPoison(int32 chance, std::function<bool(int32)> const& roll);
}

#endif
