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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_BLESSEDRESILIENCE_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_BLESSEDRESILIENCE_H

#include "spells/classes/priest/Priest.h"

/**
 * @file BlessedResilience.h
 * @brief Blessed Resilience, the Holy talent (33142, 33145): which hits the priest takes give it: the registration of
 * BlessedResilience.cpp.
 *
 * BlessedResilience.cpp registers a check-proc row on each of the talent's two ranks: a hit he takes gives Blessed
 * Resilience (33143: his rank's 15% or 30% more healing taken, for 10 seconds) when it is critical, whatever its
 * damage, or when its damage after his absorbs is greater than the percent of his maximum health the first rank's
 * effect 1 holds (10 in the client's data), the rule of Focused Will (Priest::FocusedWillTakes); every other hit is
 * refused. The proc itself is the proc-trigger aura's own, which gives the buff its rank's percent. No periodic tick
 * reaches the talent: the proc flags of the client's data name none, as Focused Will's do.
 */

namespace Priest
{
    /// Registers Blessed Resilience's check-proc row on its two ranks on `registry`; answers 2, the rows registered.
    uint32 RegisterBlessedResilience(SpellHandlerRegistry& registry);
}

#endif
