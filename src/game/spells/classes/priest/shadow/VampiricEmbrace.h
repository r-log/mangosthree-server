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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_VAMPIRICEMBRACE_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_VAMPIRICEMBRACE_H

#include "spells/classes/priest/Priest.h"

/**
 * @file VampiricEmbrace.h
 * @brief Vampiric Embrace: which hits heal and by how much: the leaves of VampiricEmbrace.cpp.
 *
 * VampiricEmbrace.cpp registers one row, the on-proc row of the aura (15286), which replaces the legacy dummy-proc
 * handler the engine's table held for it. The aura's proc flags take the priest's successful harmful spell hits and
 * his periodic damage, and its spell_proc_event row keeps them to the Shadow school, so the script sees Shadow damage
 * he deals. It refuses damage he deals himself and the damage of an area spell (VampiricEmbraceHeals) and, for any
 * other hit that dealt damage, casts the heal (15290) at himself, triggered by the aura: its effect 0 heals the party
 * around him, him among them, and its effect 1 heals him. The aura's effect 0 value is the percent of the damage he is
 * healed for (6 in the client's data, "$15286s1%"); the party is healed for half of it ("$/2;15286s1%"):
 * VampiricEmbracePartyHeal on effect 0, and VampiricEmbraceSelfHeal on effect 1 the rest of the whole percent, so that
 * he is healed the whole percent in all. Only the damage of the priest himself reaches the aura: a pet's or a
 * Shadowy Apparition's damage is its own event, which no aura of the priest sees.
 */

namespace Priest
{
    /// Registers Vampiric Embrace's on-proc row (15286) on `registry`; answers 1, the row registered.
    uint32 RegisterVampiricEmbrace(SpellHandlerRegistry& registry);

    /// Whether a hit or a tick that dealt `damage` heals: it did not hurt the priest himself (`selfDamage`), no area
    /// spell dealt it (`areaSpell`, the engine's IsAreaOfEffectSpell) and it dealt damage.
    bool VampiricEmbraceHeals(bool selfDamage, bool areaSpell, uint32 damage);

    /// What the party is healed for by a hit of `damage` at `percent` of the damage: half of the percent
    /// (`percent` x `damage` / 200, the fraction dropped); 0 for a percent of 0 or less.
    int32 VampiricEmbracePartyHeal(int32 percent, uint32 damage);

    /// What the priest is healed for by the heal's effect 1: the whole percent of the damage (`percent` x `damage` /
    /// 100, the fraction dropped) less the party's share, which the heal's effect 0 gives him as well; 0 for a percent
    /// of 0 or less.
    int32 VampiricEmbraceSelfHeal(int32 percent, uint32 damage);
}

#endif
