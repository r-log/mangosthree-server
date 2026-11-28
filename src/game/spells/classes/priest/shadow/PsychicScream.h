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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_PSYCHICSCREAM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_PSYCHICSCREAM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file PsychicScream.h
 * @brief Psychic Scream: the root its third effect holds only for a priest with the glyph: the leaves of
 * PsychicScream.cpp.
 *
 * Psychic Scream (8122) holds a fear at effect 0, a speed aura at effect 1 and a root (aura 26) at effect 2, whose
 * value in the client's data is 0. Its text reads "$?s55676[tremble in place][flee]" and its aura text
 * "$?$w3!=0[Trembling in Fear.][Running in Fear.]": a target trembles in place, feared and rooted, only while the
 * effect's value is not 0, and Glyph of Psychic Scream (55676) alone makes it 1, through its third effect, a flat
 * addition of 1 to the third effect of the priest's Psychic Scream (SPELLMOD_EFFECT3). The retail 4.3.4 sniff (build
 * 15595, 2012-05-28) shows all 65 sniffed screams apply effects 0 and 1 only. PsychicScream.cpp registers one row, the
 * aura-apply row of 8122: at its call before the holder is added to its target it takes effect 2 off the holder when
 * the effect's amount is 0 (PsychicScreamKeepsRoot), so a scream of a priest without the glyph never roots, and one
 * with it roots as it fears.
 */

namespace Priest
{
    /// Registers Psychic Scream's aura-apply row on `registry`; answers 1, the rows registered.
    uint32 RegisterPsychicScream(SpellHandlerRegistry& registry);

    /// Whether Psychic Scream's third effect keeps its root: its amount, after the caster's spell modifiers, is above
    /// 0. The client's value is 0, and the glyph's flat addition of 1 is the only thing that makes it more.
    bool PsychicScreamKeepsRoot(int32 amount);
}

#endif
