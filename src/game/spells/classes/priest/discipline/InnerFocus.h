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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_INNERFOCUS_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_INNERFOCUS_H

#include "spells/classes/priest/Priest.h"

/**
 * @file InnerFocus.h
 * @brief Inner Focus, Strength of Soul and Train of Thought: which casts take Inner Focus, which immunity a rank of
 * Strength of Soul gives and which heals shorten Weakened Soul, and which cooldown each spell of Train of Thought
 * shortens, by how much and how often: the leaves of InnerFocus.cpp.
 *
 * InnerFocus.cpp registers eleven rows. At the after-cast hook of each spell Inner Focus's mods take (2061, 101062,
 * 2060, 596, 32546; Priest::InnerFocusTakes reads the mods' class masks), a cast that finished, its cost and its hits
 * having taken the mods, takes Inner Focus (89485) off its caster, which starts its cooldown. At the aura-apply hook
 * of Inner Focus, the priest casts his Strength of Soul rank's immunity at himself (StrengthOfSoulImmunity: 96266,
 * 96267). At the check-proc hook of the talent's two ranks (89488, 89489), only Heal, Greater Heal and Flash Heal
 * (StrengthOfSoulFrom) go on to trigger its effect, and at the dummy-effect hook of that effect (89490) the healed
 * unit's Weakened Soul (6788) loses the rank's seconds (WeakenedSoulCut). At the proc hook of Train of Thought's two
 * ranks (92295, 92297), a hit of Greater Heal or of Smite shortens the cooldown TrainOfThoughtCooldown names by
 * TrainOfThoughtCut, at the rank's TrainOfThoughtChance.
 */

namespace Priest
{
    /// Registers Inner Focus's, Strength of Soul's and Train of Thought's rows on `registry`: the after-cast row of
    /// each of the five spells Inner Focus takes, Inner Focus's aura-apply row, the check-proc row of each of Strength
    /// of Soul's two ranks, its effect's dummy-effect row and the proc row of each of Train of Thought's two ranks;
    /// answers 11, the rows registered.
    uint32 RegisterInnerFocus(SpellHandlerRegistry& registry);

    /// Whether Inner Focus's mods take `spell`: one of the class masks of Inner Focus's (89485) effects in the client's
    /// data covers the spell's family flags, in Inner Focus's family. Of the priest's spells, Flash Heal (2061, and
    /// the instant 101062), Greater Heal (2060), Prayer of Healing (596) and Binding Heal (32546); not Heal (2050).
    /// False when the store lacks Inner Focus.
    bool InnerFocusTakes(SpellEntry const& spell);

    /// The immunity a rank of Strength of Soul gives when Inner Focus is applied: 96266 for the first rank (89488),
    /// 96267 for the second (89489); 0 for any other spell.
    uint32 StrengthOfSoulImmunity(uint32 rankSpellId);

    /// The rank of Strength of Soul `priest` holds: the second (89489) when he holds it, else the first (89488) when he
    /// holds that; 0 when he holds neither.
    uint32 StrengthOfSoulRank(Unit const& priest);

    /// Whether a heal of the spell `spellId` triggers Strength of Soul's effect: Heal (2050), Greater Heal (2060) and
    /// Flash Heal (2061, and the instant 101062), as the talent's tooltip names them; no other spell.
    bool StrengthOfSoulFrom(uint32 spellId);

    /// The milliseconds Weakened Soul loses for a rank whose effect 0 value is `seconds` (2 for 89488, 4 for 89489):
    /// `seconds` x 1000, held to INT32_MAX; 0 for 0 seconds or less.
    int32 WeakenedSoulCut(int32 seconds);

    /// The cooldown Train of Thought shortens for a hit of the spell `spellId`: Inner Focus's (89485) for Greater Heal
    /// (2060), Penance's (47540) for Smite (585); 0 for any other spell.
    uint32 TrainOfThoughtCooldown(uint32 spellId);

    /// The milliseconds Train of Thought takes off the cooldown of `cooldownSpellId`, from 92295's effect values in the
    /// client's data, which both ranks' tooltips read: Inner Focus's, effect 0's `seconds` (5) x 1000; Penance's,
    /// effect 1's `penanceMs` (500). Held to [0, INT32_MAX]; 0 for any other cooldown.
    int32 TrainOfThoughtCut(uint32 cooldownSpellId, int32 seconds, int32 penanceMs);

    /// The chance in percent a rank of Train of Thought shortens a cooldown at, as each rank's tooltip reads it: the
    /// first rank's (92295) effect 2 value, `effectTwoValue` (50); the second rank's (92297) proc chance, `procChance`
    /// (100). 0 for any other spell.
    int32 TrainOfThoughtChance(uint32 rankSpellId, int32 effectTwoValue, int32 procChance);
}

#endif
