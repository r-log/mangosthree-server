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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHIELD_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHIELD_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Shield.h
 * @brief Power Word: Shield's amount and what its end gives its caster: the leaves of Shield.cpp.
 *
 * Shield.cpp registers two rows on Power Word: Shield (17): at the aura-amount hook its whole amount is reckoned,
 * raised by Improved Power Word: Shield, Twin Disciplines and the Shield Discipline mastery (ShieldAmount,
 * AbsorbMastery), and at the aura-remove hook a shield used up or dispelled gives its caster Rapture's mana
 * (RaptureFires, RaptureMana), once in the energize's recovery time, which his cooldown store keeps.
 */

namespace Priest
{
    /// Registers Power Word: Shield's aura-amount and aura-remove rows on `registry`; answers 2, the rows registered.
    uint32 RegisterShield(SpellHandlerRegistry& registry);

    /// The percent the Shield Discipline mastery (77484) adds to `caster`'s absorbs: the effect 0 amount of his holder
    /// of it, which the engine keeps at the effect's bonus coefficient (2.5) times his mastery points, the fraction
    /// dropped (the 8 points of the Mastery passive give 20); 0 when he holds none.
    int32 AbsorbMastery(Unit const& caster);

    /// The amount of a Power Word: Shield whose base and spell power part come to `amount`, raised by
    /// `improvedPercent` (Improved Power Word: Shield's rank), `twinPercent` (Twin Disciplines' rank, its healing done)
    /// and `masteryPercent` (AbsorbMastery), each a factor of its own, truncated once, the product the client's tooltip
    /// takes; a percent of 0 or less raises nothing, and an amount of 0 or less stays as it is.
    int32 ShieldAmount(int32 amount, int32 improvedPercent, int32 twinPercent, int32 masteryPercent);

    /// Whether a Power Word: Shield removed in `mode` gives its caster Rapture's mana: one whose last point damage used
    /// up (AURA_REMOVE_BY_SHIELD_BREAK) or one dispelled (AURA_REMOVE_BY_DISPEL); no other removal.
    bool RaptureFires(AuraRemoveMode mode);

    /// The mana Rapture gives a priest of `maxMana` maximum mana at the rank whose effect 0 holds `percent`: that
    /// percent of it, truncated; 0 for a percent of 0 or less.
    int32 RaptureMana(uint32 maxMana, int32 percent);
}

#endif
