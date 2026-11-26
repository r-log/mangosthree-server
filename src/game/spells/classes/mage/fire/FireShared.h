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

#ifndef MANGOSSERVER_SPELLS_CLASSES_MAGE_FIRE_FIRESHARED_H
#define MANGOSSERVER_SPELLS_CLASSES_MAGE_FIRE_FIRESHARED_H

#include "spells/classes/mage/Mage.h"

/**
 * @file FireShared.h
 * @brief What two of the Fire folder's mechanic files share: Ignite's DoT (Ignite.cpp, Combustion.cpp), Frostfire Bolt
 * and its glyph (FrostfireBolt.cpp, Pyromaniac.cpp), and Pyromaniac's count at a DoT's apply, which Pyromaniac.cpp
 * defines and Frostfire Bolt's apply (FrostfireBolt.cpp) runs too.
 *
 * The spell ids stand at namespace scope, as they stood in the anonymous namespace of the one file they came from,
 * so the files that share them name them as before; the helpers are in namespace Mage, as the class's other leaves
 * are. Only this folder's files include this header.
 */

/// Ignite's damage over time, the spell the talent's ranks cast.
uint32 const IGNITE_DOT = 12654;

/// Frostfire Bolt, and Glyph of Frostfire, whose effect 2 holds the percent of a bolt's hit the bolt's DoT deals.
uint32 const FROSTFIRE_BOLT_SPELL = 44614;
uint32 const GLYPH_OF_FROSTFIRE = 61205;

namespace Mage
{
    /// 2120, 11366, 12654, 44457, 83853, 88148, 92315: the DoTs Pyromaniac counts, when their holder is added:
    /// the caster's burning enemies are counted again with the new holder on its target (PyromaniacRecount). The call
    /// before the holder is added counts nothing. The holder's boosts still run.
    SpellHandlerOutcome<void> PyromaniacDotApply(AuraApplyContext& ctx);
}

#endif
