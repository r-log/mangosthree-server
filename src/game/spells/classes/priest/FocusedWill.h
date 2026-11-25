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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_FOCUSEDWILL_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_FOCUSEDWILL_H

#include "spells/classes/priest/Priest.h"

/**
 * @file FocusedWill.h
 * @brief Focused Will: which hits the priest takes give it: the leaf of FocusedWill.cpp.
 *
 * FocusedWill.cpp registers a check-proc row on each of the talent's two ranks (45234, 45243): a hit he takes gives
 * Focused Will when it is critical or when its damage is greater than the percent of his maximum health the first
 * rank's effect 1 holds (FocusedWillTakes); every other hit is refused. The proc itself is the proc-trigger aura's own.
 */

namespace Priest
{
    /// Registers Focused Will's check-proc row on its two ranks on `registry`; answers 2, the rows registered.
    uint32 RegisterFocusedWill(SpellHandlerRegistry& registry);

    /// Whether a hit of `procEx` that dealt `damage` (after the absorbs) to a priest of `maxHealth` maximum health
    /// gives Focused Will: a critical hit, whatever its damage, or a normal hit whose damage is greater than `percent`
    /// of his maximum health; a percent of 0 or less lets no normal hit through, and no other kind of hit gives it.
    bool FocusedWillTakes(uint32 procEx, uint32 damage, uint32 maxHealth, int32 percent);
}

#endif
