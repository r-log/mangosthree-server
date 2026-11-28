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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_MINDSPIKE_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_MINDSPIKE_H

#include "spells/classes/priest/Priest.h"

/**
 * @file MindSpike.h
 * @brief Mind Blast's and Mind Spike's after-hit rows: the orbs they spend, Mind Spike's debuff Mind Blast uses up and
 * the damage over time Mind Spike puts out: the leaves of MindSpike.cpp.
 *
 * MindSpike.cpp registers two rows, the after-hit hook of each spell, because both spells need both rows of the file
 * under one key. After a hit of Mind Blast (8092) the priest's Shadow Orbs are spent (Priest::ConsumeShadowOrbs) and
 * his own Mind Spike debuff (87178), which raised the crit chance of this Mind Blast by its stacks (the engine's
 * crit-from-caster read), is taken off the target. After a hit of Mind Spike (73510) the orbs are spent and his own
 * Shadow Word: Pain, Devouring Plague and Vampiric Touch (MindSpikeExtinguishes) are taken off the target; the debuff
 * itself is Mind Spike's second effect's trigger (87178), which the engine casts. A cast that missed has no hit, so it
 * spends nothing and extinguishes nothing.
 */

namespace Priest
{
    /// Registers the after-hit rows of Mind Blast (8092) and Mind Spike (73510) on `registry`; answers 2, the rows
    /// registered.
    uint32 RegisterMindSpike(SpellHandlerRegistry& registry);

    /// Whether Mind Spike takes the caster's own aura of the spell `spellId` off its target: Shadow Word: Pain (589),
    /// Devouring Plague (2944) and Vampiric Touch (34914). Mind Flay, which is channelled, and every other spell are
    /// left.
    bool MindSpikeExtinguishes(uint32 spellId);
}

#endif
