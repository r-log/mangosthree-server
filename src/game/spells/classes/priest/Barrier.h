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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_BARRIER_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_BARRIER_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Barrier.h
 * @brief Power Word: Barrier and Glyph of Power Word: Barrier: the barrier's creature, how long it stands and which
 * of its periodic casts the glyph switches: the leaves of Barrier.cpp.
 *
 * Power Word: Barrier (62618) has one effect in the 4.3.4 client's data: the summon of the barrier, creature 33248,
 * at the point it is cast at. Barrier.cpp registers three rows and the barrier's creature AI:
 *  - a select-targets row on 62618: the summon's effect takes the priest alone, so that one cast summons at the point
 *    whoever stands there (the effect's targets are the friendly units within 7 yards of it, and the summon runs once
 *    for each);
 *  - an effect-value row on 62618: the summon's value, which the spell's scaling row raises to 21225 at level 85 and
 *    which the summon takes as its count of creatures, becomes one (BarrierSummonValue), so that the summon makes
 *    one barrier;
 *  - BarrierAI, chosen by its permit for creature 33248 alone (IsBarrierCreature). On its first update a barrier
 *    that 62618 made takes its summoner, the priest, as its owner and its creator and his faction as its own, and
 *    channels 81781 at itself; the aura's data does the rest: every 100 ms its effect 0 casts 81782 (damage taken
 *    -25%, pushback resisted, 500 ms) and its effect 1 casts 90785 (healing taken +10%, 400 ms), each at the units
 *    friendly to the barrier within 7 yards of it. The barrier stands for 62618's duration (10 s) and goes earlier
 *    when its priest is dead or not on its map (BarrierStands);
 *  - a periodic-tick row on 81781: the tick of the effect whose trigger spell is 90785 casts nothing while the
 *    barrier's priest does not hold Glyph of Power Word: Barrier (55689) (BarrierTickCasts).
 * 81781, 81782 and 90785 share one icon; SpellMgr::IsNoStackSpellDueToSpell lets 81782 and 90785 stand together on
 * one unit. 81781 stands on the barrier alone, which neither of them reaches: 81782 cannot target its caster, and the
 * barrier cannot be selected, which keeps it out of every area fill.
 */

namespace Priest
{
    /// Registers the barrier's rows on `registry`, its select-targets and effect-value rows on 62618 and its
    /// periodic-tick row on 81781, and the barrier's creature AI, whose factory is added to the creature AI registry
    /// once; answers 3, the rows registered.
    uint32 RegisterBarrier(SpellHandlerRegistry& registry);

    /// Whether `creatureEntry` is Power Word: Barrier's creature (33248).
    bool IsBarrierCreature(uint32 creatureEntry);

    /// Whether a tick of the barrier's aura (81781) casts its trigger spell `triggerSpell`: every trigger spell but
    /// the glyph's (90785), which it casts only while the barrier's priest holds the glyph (`glyph`).
    bool BarrierTickCasts(uint32 triggerSpell, bool glyph);

    /// Whether a barrier stands on: while it has life left (`lifeMs` above 0) and its priest is alive on its map
    /// (`priest`).
    bool BarrierStands(int32 lifeMs, bool priest);

    /// The value of an effect of 62618 of kind `effect` (SpellEffects) whose calculated value is `value`: one for the
    /// summon (SPELL_EFFECT_SUMMON), the count of barriers it makes; `value` for any other kind.
    int32 BarrierSummonValue(uint32 effect, int32 value);
}

#endif
