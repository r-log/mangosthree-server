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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_DIVINEAEGIS_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_DIVINEAEGIS_H

#include "spells/classes/priest/Priest.h"

/**
 * @file DivineAegis.h
 * @brief Divine Aegis: which heals make the shield and how large it is: the leaves of DivineAegis.cpp.
 *
 * DivineAegis.cpp registers two rows on each of the talent's three ranks (47509, 47511, 47515): at the check-proc hook
 * a heal that makes a shield is let through and every other event is refused (AegisFromHeal), and at the proc hook
 * the priest casts the shield (47753) at the unit he healed, its amount his rank's percent of the heal raised by his
 * Shield Discipline mastery (AegisShield), added onto what is left of his own shield on that unit and held to his cap
 * (AegisAmount, AegisCap). Another caster's shield on the unit is its own holder and is not touched.
 */

namespace Priest
{
    /// Registers Divine Aegis's check-proc and proc rows on its three ranks on `registry`; answers 6, the rows
    /// registered.
    uint32 RegisterDivineAegis(SpellHandlerRegistry& registry);

    /// Whether an event of `procFlag` and `procEx` for the spell `spellId` that healed `amount` makes a Divine Aegis
    /// shield: a heal of the priest's (a helpful spell's hit, or a heal over time's tick) that healed something, and
    /// either a critical heal or any heal of Prayer of Healing (596). A hit that healed nothing, a cast's end, a
    /// harmful spell's hit and a damage over time's tick make none.
    bool AegisFromHeal(uint32 procFlag, uint32 procEx, uint32 spellId, uint32 amount);

    /// The shield a heal of `heal` makes: `rankPercent` (the rank's effect 0 percent: 10, 20 or 30) of the heal,
    /// raised by `masteryPercent` (Priest::AbsorbMastery), truncated once and held to the largest int32; 0 for a heal
    /// or a rank percent of 0 or less, and a mastery percent of 0 or less raises nothing.
    int32 AegisShield(uint32 heal, int32 rankPercent, int32 masteryPercent);

    /// The most the priest's own Divine Aegis on one unit holds once a shield of his adds onto it: 40 percent of his
    /// maximum health `maxHealth`, truncated.
    int32 AegisCap(uint32 maxHealth);

    /// The amount of the shield a new one of `fresh` makes on a unit where `left` is what is left of the priest's own
    /// shield: their sum, held to `cap` when it is larger, but never below `fresh`, so a new shield alone is never cut;
    /// `fresh` when he holds none there (`left` 0 or less). A `fresh` of 0 or less is returned as it is.
    int32 AegisAmount(int32 fresh, int32 left, int32 cap);
}

#endif
