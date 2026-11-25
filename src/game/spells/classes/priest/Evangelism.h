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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_EVANGELISM_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_EVANGELISM_H

#include "spells/classes/priest/Priest.h"

/**
 * @file Evangelism.h
 * @brief Evangelism and Archangel: which hits give a stack, which stack a rank gives, the stacks a priest holds and
 * what Archangel makes of them: the leaves of Evangelism.cpp.
 *
 * Evangelism.cpp registers seven rows. At the check-proc and proc hooks of the talent's two ranks (81659, 81662), a hit
 * of the priest's Smite, Holy Fire or Penance's bolt that dealt damage (EvangelismFromHit) gives him a stack of his
 * rank's buff (EvangelismStack: 81660, 81661), whose effects raise the damage of those spells and lower their cost for
 * each stack, and with it the marker (87154) Archangel asks of its caster. At the aura-remove hook of the two buffs,
 * the marker leaves him with the last stack of either kind of Evangelism (HoldsEvangelism). At the dummy-effect hook of
 * Archangel (87151), which its script effect dispatches, his stacks (EvangelismStacks) are consumed for the healing
 * bonus (81700) and the mana (87152), each its effect 0 value for each stack (ArchangelPoints).
 */

namespace Priest
{
    /// Registers Evangelism's and Archangel's rows on `registry`: the check-proc and proc rows of each of the talent's
    /// two ranks, the aura-remove row of each rank's buff and Archangel's dummy-effect row; answers 7, the rows
    /// registered.
    uint32 RegisterEvangelism(SpellHandlerRegistry& registry);

    /// Whether a hit of the spell `spellId` gives a stack of Evangelism: Smite (585), Holy Fire (14914) and Penance's
    /// bolt at an enemy (47666); no other spell, Mind Flay's Dark Evangelism among them.
    bool EvangelismFromSpell(uint32 spellId);

    /// Whether the event of `procFlag` and `procEx` of the spell `spellId`, which dealt `damage`, gives a stack: a
    /// harmful spell's hit (PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT), normal or critical, of a spell
    /// EvangelismFromSpell names, that dealt damage. A periodic tick, a miss, a hit that dealt nothing and a cast's end
    /// do not.
    bool EvangelismFromHit(uint32 spellId, uint32 procFlag, uint32 procEx, uint32 damage);

    /// The buff a stack of the talent's rank `rankSpellId` is: 81660 for the first rank (81659), 81661 for the second
    /// (81662); 0 for any other spell.
    uint32 EvangelismStack(uint32 rankSpellId);

    /// The stacks `priest` holds of his own buff of either rank (81660, 81661, which never stand together); 0 when he
    /// holds neither.
    uint32 EvangelismStacks(Unit const& priest);

    /// Whether `priest` holds a stack of either kind of Evangelism: a rank's buff (81660, 81661) or Dark Evangelism's
    /// (87117, 87118), whose stacks the same marker stands for.
    bool HoldsEvangelism(Unit const& priest);

    /// The value Archangel casts one of its spells with for `stacks` stacks, that spell's effect 0 value being
    /// `perStack`: their product, held to INT32_MAX; 0 for no stack or a value of 0 or less.
    int32 ArchangelPoints(int32 perStack, uint32 stacks);
}

#endif
