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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_MASOCHISM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_MASOCHISM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Masochism.h
 * @brief Masochism: which events the priest takes give him mana: the leaf of Masochism.cpp.
 *
 * Masochism.cpp registers a check-proc row on each of the talent's two ranks (88994, 88995), which have no
 * spell_proc_event row, so the script's answer decides: an event he takes gives the aura's trigger (Masochism, 89007,
 * with the rank's 5 or 10 percent of his maximum mana) when it is his own Shadow Word: Death's recoil tick or a hit
 * that is no periodic tick and whose damage after his absorbs is at least the percent of his maximum health the first
 * rank's effect 1 holds (MasochismTakes); every other event is refused, the other periodic ticks among them.
 */

namespace Priest
{
    /// Registers Masochism's check-proc row on its two ranks on `registry`; answers 2, the rows registered.
    uint32 RegisterMasochism(SpellHandlerRegistry& registry);

    /// Whether an event he takes gives Masochism: the periodic damage of `spellId` (`procFlag` holds
    /// PROC_FLAG_ON_TAKE_PERIODIC) when it is Shadow Word: Death's recoil (32409), of any size, a tick that dealt
    /// nothing included; any other periodic event, never; an event that is no tick when `damage`, after his absorbs,
    /// is at least `percent` of `maxHealth`, the hit's kind and its spell not asked; a percent of 0 or less lets no
    /// hit through.
    bool MasochismTakes(uint32 spellId, uint32 procFlag, uint32 damage, uint32 maxHealth, int32 percent);
}

#endif
