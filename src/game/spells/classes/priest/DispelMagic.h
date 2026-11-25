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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_DISPELMAGIC_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_DISPELMAGIC_H

#include "spells/classes/priest/Priest.h"

/**
 * @file DispelMagic.h
 * @brief Dispel Magic, Absolution and Glyph of Dispel Magic: at which units the priest may cast Dispel Magic, which
 * dispel it casts at each, and when and how much the glyph heals: the leaves of DispelMagic.cpp.
 *
 * DispelMagic.cpp registers two rows on Dispel Magic (527). Its check-cast row refuses a cast at a friendly unit other
 * than the priest himself while he does not hold Absolution (33167) (DispelMagicRefused). Its dummy-effect row has
 * the priest cast, triggered, at the unit the dummy lands on, the dispel the 4.3.4 client's data and the retail sniff
 * name for it (DispelMagicSpell): 97690 at himself or a friendly unit, which takes two harmful magic effects off it,
 * 97691 at any other unit, which takes one helpful magic effect off it. With Glyph of Dispel Magic (55677) on him, a
 * 97690 that took at least one effect off (DummyEffectContext::CastTriggeredDispel, GlyphOfDispelMagicHeals) is
 * followed by 56131 at the same unit, healing it for the glyph's effect 0 value (3) in percent of its maximum health
 * (GlyphOfDispelMagicHeal). Mass Dispel (32375) is another spell, and none of this touches it.
 */

namespace Priest
{
    /// Registers Dispel Magic's rows on `registry`: its check-cast row and its dummy-effect row; answers 2, the rows
    /// registered.
    uint32 RegisterDispelMagic(SpellHandlerRegistry& registry);

    /// Whether Dispel Magic is refused at a unit: one friendly to the priest (`friendly`) other than himself (`self`
    /// false), while he does not hold Absolution (`absolution` false). Never at himself, nor at a unit not friendly to
    /// him.
    bool DispelMagicRefused(bool self, bool friendly, bool absolution);

    /// The dispel Dispel Magic casts at a unit: 97690 at the priest himself or a unit friendly to him (`friendly`),
    /// 97691 at any other.
    uint32 DispelMagicSpell(bool friendly);

    /// Whether Glyph of Dispel Magic heals after Dispel Magic's dispel `dispelSpellId` took `taken` stacks off its
    /// unit: only after the friendly dispel (97690), only when it took at least one, and only for a priest who holds
    /// the glyph (`glyph`).
    bool GlyphOfDispelMagicHeals(uint32 dispelSpellId, uint32 taken, bool glyph);

    /// The heal of Glyph of Dispel Magic at a unit of `maxHealth`: `percent` (the glyph's effect 0 value, 3) percent
    /// of it, rounded down, held to INT32_MAX; 0 for a percent of 0 or less.
    int32 GlyphOfDispelMagicHeal(uint32 maxHealth, int32 percent);
}

#endif
