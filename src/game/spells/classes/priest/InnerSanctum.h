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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_INNERSANCTUM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_INNERSANCTUM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file InnerSanctum.h
 * @brief Inner Sanctum's Spell Warding: which rank of the talent a priest holds and the points Spell Warding takes
 * from it: the leaves of InnerSanctum.cpp.
 *
 * InnerSanctum.cpp registers two rows. At the aura-apply hook of Inner Fire (588), the priest casts Spell Warding
 * (91724) at himself, its effect 0 (spell damage taken, every magic school) at SpellWardingPoints of the effect 0
 * value of the Inner Sanctum rank he holds (InnerSanctumRank: 14747, 14770, 14771); at Inner Fire's aura-remove hook,
 * Spell Warding leaves him with it. The talent's effect 1, Inner Will's speed, is a spell modifier the engine applies
 * on its own.
 */

namespace Priest
{
    /// Registers Inner Sanctum's rows on `registry`: Inner Fire's aura-apply row and its aura-remove row; answers 2,
    /// the rows registered.
    uint32 RegisterInnerSanctum(SpellHandlerRegistry& registry);

    /// The rank of Inner Sanctum `priest` holds: the third (14771) when he holds it, else the second (14770), else the
    /// first (14747); 0 when he holds none.
    uint32 InnerSanctumRank(Unit const& priest);

    /// Spell Warding's effect 0 points for a rank whose effect 0 value is `rankValue` (2, 4, 6 in the client's data):
    /// -`rankValue`, the percent less spell damage taken, held to -100; 0 for a value of 0 or less.
    int32 SpellWardingPoints(int32 rankValue);
}

#endif
