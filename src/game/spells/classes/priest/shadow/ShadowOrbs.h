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

#ifndef MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWORBS_H
#define MANGOSSERVER_SPELLS_CLASSES_PRIEST_SHADOWORBS_H

#include "spells/classes/priest/Priest.h"

/**
 * @file ShadowOrbs.h
 * @brief Shadow Orbs, the Shadow mastery: which ticks give an orb, what the orbs add to Mind Blast and Mind Spike and
 * what spending them gives: the leaves of ShadowOrbs.cpp.
 *
 * ShadowOrbs.cpp registers two rows. At the check-proc hook of Shadow Orb Power (77486), whose proc flags take the
 * priest's harmful hits and his periodic damage, a damage tick of Shadow Word: Pain or Mind Flay (OrbFromTick) is
 * answered true and every other event false; with no spell_proc_event row for the aura, a true answer skips the class
 * mask and the hit kind of the engine's check, and the chance that rolls is the client's 10 plus the modifier
 * Harnessed Shadows adds to it, both the engine's own. At the damage-done hook of the orbs (77487), whose first effect
 * is a dummy aura, so the row runs for every spell the priest deals, a direct hit of Mind Blast or Mind Spike
 * (SpendsShadowOrbs) deals 1 + the orbs x the orb's percent (ShadowOrbMultiplier). The percent (ShadowOrbPercent) is
 * Empowered Shadow's effect 0 value, 10 in the client's data, plus Shadow Orb Power's effect 0 coefficient, 1.45,
 * times his mastery points (PLAYER_MASTERY), the fraction kept, as Echo of Light reads the Holy mastery; never the
 * whole number the orbs' amount holds. ConsumeShadowOrbs, which the after-hit rows of both spells (MindSpike.cpp) call,
 * takes the orbs off him and casts Empowered Shadow (95799) at him with the percent rounded on its two effects
 * (EmpoweredShadowPoints): one holder whatever the orbs were.
 */

namespace Priest
{
    /// Registers the orbs' rows on `registry`: the check-proc row of Shadow Orb Power (77486) and the damage-done row
    /// of the orbs (77487); answers 2, the rows registered.
    uint32 RegisterShadowOrbs(SpellHandlerRegistry& registry);

    /// Whether the event of `procFlag` for the spell `spellId`, which dealt `damage`, may give an orb: a periodic
    /// damage tick (PROC_FLAG_ON_DO_PERIODIC) of Shadow Word: Pain (589) or Mind Flay (15407) that dealt damage. The
    /// hits of Mind Blast, Shadow Word: Death, Vampiric Touch and Devouring Plague, their ticks, Mind Sear's (49821)
    /// and a tick that dealt nothing do not.
    bool OrbFromTick(uint32 spellId, uint32 procFlag, uint32 damage);

    /// Whether a hit of the spell `spellId` spends the priest's orbs: Mind Blast (8092) and Mind Spike (73510).
    bool SpendsShadowOrbs(uint32 spellId);

    /// The percent one Shadow Orb adds to a hit of `caster`, and Empowered Shadow adds to his periodic damage:
    /// Empowered Shadow's (95799) effect 0 value plus Shadow Orb Power's (77486) effect 0 coefficient times his
    /// mastery points (PLAYER_MASTERY), the fraction kept (8 points give 21.6). 0 for a unit that is not a player and
    /// when the store lacks either spell.
    float ShadowOrbPercent(Unit const& caster);

    /// The stacks of Shadow Orb (77487) `caster` holds of his own; 0 for none.
    uint32 ShadowOrbs(Unit const& caster);

    /// What `stacks` orbs at `percent` each do to a hit of `spellId` that is a direct hit (`damageType`
    /// SPELL_DIRECT_DAMAGE): 1 + `stacks` x `percent` / 100 for a hit that spends them, else 1.
    float ShadowOrbMultiplier(uint32 spellId, DamageEffectType damageType, uint32 stacks, float percent);

    /// `percent` as the whole number Empowered Shadow's two effects hold: rounded to the nearest, a half up; 0 for a
    /// percent of 0 or less.
    int32 EmpoweredShadowPoints(float percent);

    /// After a hit that spends the orbs: when the caster holds any, Shadow Orb is taken off him and Empowered Shadow
    /// (95799) is cast at him, triggered by the hit, with ShadowOrbPercent rounded on both of its effects. Nothing for
    /// a caster with no orbs.
    void ConsumeShadowOrbs(AfterHitContext& ctx);
}

#endif
