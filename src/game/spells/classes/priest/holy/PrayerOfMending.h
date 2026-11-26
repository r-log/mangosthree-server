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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_PRAYEROFMENDING_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_PRAYEROFMENDING_H

#include "spells/classes/priest/Priest.h"

class Unit;

/**
 * @file PrayerOfMending.h
 * @brief Prayer of Mending, the Holy spell (33076): what its aura (41635) does when its holder is hit, and Glyph of
 * Prayer of Mending (55685): the leaves of PrayerOfMending.cpp.
 *
 * Prayer of Mending casts its aura (41635, 5 charges, 30 seconds) at its target with a value, which the aura keeps as
 * its amount. Aura type 225, whose proc handler the engine kept in UnitAuraProcHandler.cpp, is used by that aura alone,
 * so PrayerOfMending.cpp holds the handler: an on-proc row on 41635. When the holder is hit, the row heals the holder
 * with Prayer of Mending's heal (33110) for the aura's amount, cast by the holder with the priest as the original caster
 * and no triggering aura, so that the heal starts the priest's procs (Inspiration, Echo of Light); the heal's spell id
 * stays 33110, which Chakra's check-proc refuses, so a bounce never spends Chakra. The first heal of a cast, the one
 * the holder takes while the aura holds its whole count of charges, is raised by the effect 0 amount of the priest's
 * Glyph of Prayer of Mending (55685, 60 in the client's data), as a percent of the aura's amount, the heal's base value;
 * the heal's spell power part (the spell bonus row of 33110) is added after it. While charges remain after the heal,
 * the aura jumps: a new holder with one charge fewer is made for the priest on a member of the holder's group or a pet
 * of a member (OnProcContext::RandomGroupUnitNear), chosen at random, never the holder, and marked ready to proc
 * (OnProcContext::MoveAuraTo); the old holder, a single-target aura of the priest's, goes when the new one lands. A holder with no group, or with no unit of its group in reach, keeps the aura
 * and spends a charge. A jump needs a player holder or a pet holder and a player priest.
 */

namespace Priest
{
    /// Registers Prayer of Mending's row on `registry`: the on-proc row of its aura (41635); answers 1, the rows
    /// registered.
    uint32 RegisterPrayerOfMending(SpellHandlerRegistry& registry);

    /// The amount Prayer of Mending's heal takes: `amount`, the aura's, raised by `glyph` percent of the whole heal,
    /// `amount` and `spellPower` (the part the priest's spell power adds after it), when the holder's `charges` are all
    /// of the cast's, `fullCharges`, so that this is the first heal of the cast, held to the largest int32; `amount`
    /// alone for any later heal, for a glyph of 0 or less, and when `fullCharges` is 0.
    int32 MendingHealAmount(int32 amount, int32 spellPower, int32 glyph, uint32 charges, uint32 fullCharges);

    /// The charges the aura keeps on its next holder when its holder holding `charges` is hit: one fewer; 0 for 1 or 0
    /// charges, which jump nowhere.
    uint32 MendingJumpCharges(uint32 charges);

    /// The effect 0 amount of the Glyph of Prayer of Mending (55685) `priest` holds; 0 when he holds none.
    int32 MendingGlyph(Unit const& priest);
}

#endif
