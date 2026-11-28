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
 * @brief Evangelism, Dark Evangelism and Archangel: which hits give a stack, which stack a rank gives, the stacks a
 * priest holds and what Archangel makes of them: the leaves of Evangelism.cpp.
 *
 * Evangelism.cpp registers nine rows. At the check-proc and proc hooks of the talent's two ranks (81659, 81662), a hit
 * of the priest's Smite, Holy Fire or Penance's bolt, wholly absorbed or not, or the cast of his Mind Flay, its
 * harmful hit at its target (EvangelismFromHit), gives him a stack of his rank's buff: Evangelism's (EvangelismStack:
 * 81660, 81661) for the first three, whose effects raise the damage of those spells and lower their cost for each
 * stack, Dark Evangelism's (DarkEvangelismStack: 87117, 87118) for Mind Flay, whose effects raise the damage of his
 * Shadow spells for each stack; with it comes the marker (87154) Archangel asks of its caster. The stacks of the other
 * kind are taken off him (TakeOffOtherEvangelism). At the aura-remove hook of the four buffs, the marker leaves him
 * with the last stack of either kind (HoldsEvangelism). At the dummy-effect hook of Archangel (87151), which its
 * script effect dispatches, his stacks of Dark Evangelism (DarkEvangelismStacks) are consumed for Dark Archangel
 * (87153) and the mana (87152), and his stacks of Evangelism (EvangelismStacks) for the healing bonus (81700) and the
 * mana, each the client's value for each stack (ArchangelPoints).
 */

namespace Priest
{
    /// Registers Evangelism's, Dark Evangelism's and Archangel's rows on `registry`: the check-proc and proc rows of
    /// each of the talent's two ranks, the aura-remove row of each of the four buffs and Archangel's dummy-effect row;
    /// answers 9, the rows registered.
    uint32 RegisterEvangelism(SpellHandlerRegistry& registry);

    /// Whether a hit of the spell `spellId` gives a stack of Evangelism: Smite (585), Holy Fire (14914) and Penance's
    /// bolt at an enemy (47666); no other spell, Mind Flay (DarkEvangelismFromSpell) among them.
    bool EvangelismFromSpell(uint32 spellId);

    /// Whether the cast of the spell `spellId` gives a stack of Dark Evangelism: Mind Flay (15407); no other spell.
    bool DarkEvangelismFromSpell(uint32 spellId);

    /// Whether the event of `procFlag` and `procEx` of the spell `spellId` gives a stack, of Evangelism or of Dark
    /// Evangelism: a harmful spell's hit (PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT), normal or critical, of a spell
    /// EvangelismFromSpell or DarkEvangelismFromSpell names, a hit a shield wholly absorbs included (the tooltips'
    /// "when you cast"). For Mind Flay, a channel, that event is the hit of the cast at its target, once, when the
    /// channel starts and its periodic damage is applied: the event of its ticks, PROC_FLAG_ON_DO_PERIODIC, which
    /// carries no hit flag, gives none. A miss, a resist, an immunity and a cast's end do not either.
    bool EvangelismFromHit(uint32 spellId, uint32 procFlag, uint32 procEx);

    /// The buff a stack of the talent's rank `rankSpellId` is: 81660 for the first rank (81659), 81661 for the second
    /// (81662); 0 for any other spell.
    uint32 EvangelismStack(uint32 rankSpellId);

    /// The buff a stack of Dark Evangelism of the talent's rank `rankSpellId` is: 87117 for the first rank (81659),
    /// 87118 for the second (81662); 0 for any other spell.
    uint32 DarkEvangelismStack(uint32 rankSpellId);

    /// Takes off `priest` the stacks of the kind of Evangelism that a hit of the spell `hitSpellId` does not give: his
    /// Dark Evangelism (87117, 87118) for a spell EvangelismFromSpell names, his Evangelism (81660, 81661) for Mind
    /// Flay; nothing for any other spell. The marker leaves with them only when no stack of the kind that stays does.
    void TakeOffOtherEvangelism(Unit& priest, uint32 hitSpellId);

    /// The stacks `priest` holds of his own buff of either rank (81660, 81661, which never stand together); 0 when he
    /// holds neither.
    uint32 EvangelismStacks(Unit const& priest);

    /// The stacks `priest` holds of his own Dark Evangelism of either rank (87117, 87118, which never stand together);
    /// 0 when he holds neither.
    uint32 DarkEvangelismStacks(Unit const& priest);

    /// Whether `priest` holds a stack of either kind of Evangelism: a rank's buff (81660, 81661) or Dark Evangelism's
    /// (87117, 87118), whose stacks the same marker stands for.
    bool HoldsEvangelism(Unit const& priest);

    /// The value Archangel casts one of its spells with for `stacks` stacks, that spell's effect value being
    /// `perStack`: their product, held to INT32_MAX; 0 for no stack or a value of 0 or less.
    int32 ArchangelPoints(int32 perStack, uint32 stacks);
}

#endif
