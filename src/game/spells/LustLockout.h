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

#ifndef MANGOSSERVER_SPELLS_LUSTLOCKOUT_H
#define MANGOSSERVER_SPELLS_LUSTLOCKOUT_H

#include "Platform/Define.h"

class SpellHandlerRegistry;
class Unit;

/**
 * @file LustLockout.h
 * @brief The rule the haste spells of LustLockoutOf's table share (SpellTables.h): each puts its own lockout on every
 *        unit it hastes, and none of them hastes a unit that holds any of their lockouts.
 *
 * LustLockout.cpp registers two scripts under each haste spell: at the target hook, the units that hold a lockout
 * leave the effect's list; at the aura-apply hook, a unit the haste lands on casts the spell's lockout on itself,
 * unless it holds a lockout already.
 */

namespace Spells
{
    /// Whether `unit` holds the lockout of any haste spell of LustLockoutOf's table.
    bool HoldsLustLockout(Unit const& unit);
}

/// Registers the target and aura-apply scripts of every haste spell LustSpellAt names. Returns the rows registered; a
/// row whose key was taken does not change the table, so a caller that finds fewer keys than rows has a duplicate.
uint32 RegisterLustScripts(SpellHandlerRegistry& registry);

#endif
