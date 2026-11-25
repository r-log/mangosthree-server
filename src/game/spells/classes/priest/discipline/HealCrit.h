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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_HEALCRIT_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_HEALCRIT_H

#include "spells/classes/priest/Priest.h"

/**
 * @file HealCrit.h
 * @brief Renewed Hope and Glyph of Flash Heal, the priest's heal crit talents: which rank of Renewed Hope he holds,
 * which heals each of the two raises and at which targets, and the crit chance they add together: the leaves of
 * HealCrit.cpp.
 *
 * HealCrit.cpp registers five rows, all at the crit-chance hook: Flash Heal (2061) and its instant cast (101062),
 * Greater Heal (2060), Heal (2050) and Penance's heal (47750), the heals the two spells' class masks in the client's
 * data cover, Flash Heal among them for Renewed Hope by its text (RenewedHopeTakes). Each row adds HealCritBonus to
 * the heal's crit chance: the effect 0 value of the Renewed Hope rank the priest holds (57470: 5, 57472: 10) at a
 * target with Weakened Soul or with his own Grace (RenewedHopeTarget), and, for a heal Glyph of Flash Heal (55679)
 * takes, the glyph's effect 0 value (10) at a target at or under its effect 1 value in percent of its health (25).
 * Neither adds healing: the talent's text gives it crit chance alone.
 */

namespace Priest
{
    /// Registers Renewed Hope's and Glyph of Flash Heal's rows on `registry`: the crit-chance row of each of the five
    /// heals; answers 5, the rows registered.
    uint32 RegisterHealCrit(SpellHandlerRegistry& registry);

    /// The rank of Renewed Hope `priest` holds: the second (57472) when he holds it, else the first (57470); 0 when he
    /// holds neither.
    uint32 RenewedHopeRank(Unit const& priest);

    /// Whether Renewed Hope raises `spell`: the class mask of Renewed Hope's effect 0 (57470) in the client's data
    /// covers the spell's family flags, in the talent's family (Heal, Greater Heal, Penance's heal), or the spell is
    /// Flash Heal (2061, and the instant 101062), which the talent's text names and its mask leaves out. False for
    /// any other spell, and for a masked one when the store lacks Renewed Hope.
    bool RenewedHopeTakes(SpellEntry const& spell);

    /// Whether Glyph of Flash Heal raises `spell`: the class mask of the glyph's effect 0 (55679) in the client's data
    /// covers the spell's family flags, in the glyph's family (Flash Heal, 2061 and 101062). False when the store
    /// lacks the glyph.
    bool FlashHealGlyphTakes(SpellEntry const& spell);

    /// Whether `target` stands as Renewed Hope asks: it holds Weakened Soul (6788), whoever put it on, or a Grace
    /// (47930, 77613) whose caster is `priest`.
    bool RenewedHopeTarget(Unit const& priest, Unit const& target);

    /// Whether `health` is at or under `percent` percent of `maxHealth`; false for a maximum of 0 or a percent of 0
    /// or less.
    bool AtOrUnderPercent(uint32 health, uint32 maxHealth, int32 percent);

    /// The crit chance, in percent, Renewed Hope and Glyph of Flash Heal add to `priest`'s `spell` at `target`: the
    /// effect 0 value of the Renewed Hope rank he holds when the talent raises the spell and the target stands as it
    /// asks, plus, when he holds the glyph and it raises the spell, the glyph's effect 0 value at a target at or under
    /// the glyph's effect 1 value in percent of its health. Each value is read from the client's data.
    int32 HealCritBonus(Unit const& priest, Unit const& target, SpellEntry const& spell);
}

#endif
