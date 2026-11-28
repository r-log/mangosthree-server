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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_PHANTASM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_PHANTASM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Phantasm.h
 * @brief Phantasm: the chance Fade frees him from his snares and roots: the leaves of Phantasm.cpp.
 *
 * Phantasm (47569, 47570) holds a dummy effect 0 whose value, 50 and 100, is the chance in percent that Fade (586)
 * removes his snares and roots, and a proc flag that does nothing. Phantasm.cpp registers one row, the aura-apply row
 * of Fade: once the holder stands on a priest who holds a rank (PhantasmRank), a roll at the rank's chance
 * (PhantasmChance, PhantasmRolls; a chance of 100 draws none) takes every harmful root and snare off him
 * (Unit::RemoveAurasAtMechanicImmunity). The rows of the talent itself stay as they were.
 */

namespace Priest
{
    /// Registers Phantasm's aura-apply row, on Fade, on `registry`; answers 1, the rows registered.
    uint32 RegisterPhantasm(SpellHandlerRegistry& registry);

    /// The rank of the talent `priest` holds, the higher first: 47570, else 47569, else 0.
    uint32 PhantasmRank(Unit const& priest);

    /// The chance in percent that Fade frees him, of the rank `rank`: its effect 0 value in the client's data (50 and
    /// 100); 0 for a spell that is no rank of the talent, and when the store lacks it.
    int32 PhantasmChance(uint32 rank);

    /// Whether `roll`, a draw of 0 to 99, frees him at `chance` percent: a chance of 100 or more always does, a chance
    /// of 0 or less never.
    bool PhantasmRolls(int32 chance, int32 roll);
}

#endif
