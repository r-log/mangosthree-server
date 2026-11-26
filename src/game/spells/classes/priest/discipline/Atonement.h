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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_ATONEMENT_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_ATONEMENT_H

#include "spells/classes/priest/Priest.h"

#include <list>

/**
 * @file Atonement.h
 * @brief Atonement: which hits heal, whom and for how much: the leaves of Atonement.cpp.
 *
 * Atonement.cpp registers five rows. At the proc hook of the talent's two ranks (14523, 81749), a hit or a tick of
 * the priest's Smite or Holy Fire (AtonementFromSpell) that dealt damage heals the most injured friendly unit
 * (MostInjured) within the first rank's yards of the enemy's edge (FriendsAround), the priest included: his rank's
 * percent of the damage, held to a share of his own maximum health and halved when the unit healed is himself
 * (AtonementHeal, AtonementCap). The heal is the priest's own cast of 81751, or of 94472 when the damage was a
 * critical hit (AtonementHealSpell), with no triggering aura, so its hit starts the procs a heal of his starts:
 * Divine Aegis takes a critical one. The value it is cast with is AtonementBasePoints less the points the engine adds
 * to the heal's value for his level, so the heal it deals before the engine's healing modifiers is the heal reckoned
 * here. At the select-targets hook of both heals, the heal goes to its cast's target alone, and at the crit-chance
 * hook of 94472 the heal is a critical heal, cast from half the heal, which the engine's critical heal doubles back.
 */

namespace Priest
{
    /// Registers Atonement's rows on `registry`: the proc row of each of the talent's two ranks, the select-targets
    /// row of each of the two heals and the crit-chance row of the critical heal; answers 5, the rows registered.
    uint32 RegisterAtonement(SpellHandlerRegistry& registry);

    /// Whether a hit or a tick of the spell `spellId` heals through Atonement: Smite (585) and Holy Fire (14914), its
    /// hit and its ticks alike; no other spell.
    bool AtonementFromSpell(uint32 spellId);

    /// The heal spell a hit of `procEx` casts: the critical heal, 94472, when it holds PROC_EX_CRITICAL_HIT; the heal,
    /// 81751, otherwise.
    uint32 AtonementHealSpell(uint32 procEx);

    /// The most a heal of a priest of `maxHealth` maximum health heals for: 30 percent of it, truncated. The client's
    /// data holds no such number.
    int32 AtonementCap(uint32 maxHealth);

    /// The heal of a hit of `damage` (after the absorbs, the damage the event carries) at the rank whose effect 0
    /// holds `percent` (50 or 100), for a priest of `maxHealth` maximum health: that percent of the damage, truncated,
    /// held to AtonementCap, then halved, truncated, when `self`, the unit healed being the priest himself. 0 for no
    /// damage or a percent of 0 or less.
    int32 AtonementHeal(uint32 damage, int32 percent, uint32 maxHealth, bool self);

    /// The value the heal spell is cast with for a heal of `heal`: the heal itself, or half of it, truncated, when
    /// `critical`, as the engine's critical heal doubles the critical heal's value (Combat::SpellCriticalHealingBonus).
    /// A heal of 0 or less is returned as it is.
    int32 AtonementBasePoints(int32 heal, bool critical);

    /// Fills `out`, cleared first, with the units `caster` may heal through Atonement around `center`, the enemy hit:
    /// alive and targetable (as the area fill of the engine's friendly area heals takes them), no totem, in the
    /// caster's phase, friendly to him, him included, whose edge is within `radius` yards of `center`'s edge, its
    /// combat reach, then in the caster's line of sight, as Spell::CheckTarget asks of the heal's target. Empty when
    /// `center` is in no world.
    void FriendsAround(Unit const& caster, Unit const& center, float radius, std::list<Unit*>& out);

    /// The unit of `units` with the lowest health percent, a unit of 0 maximum health aside; of two at the same
    /// percent, the one of the lower raw guid. NULL when there is none.
    Unit* MostInjured(std::list<Unit*> const& units);
}

#endif
