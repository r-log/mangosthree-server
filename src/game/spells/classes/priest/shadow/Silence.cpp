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

#include "spells/classes/priest/shadow/Silence.h"
#include "entities/Unit.h"

namespace
{
    /// Silence.
    uint32 const SILENCE[] = { 15487 };

    /// The interrupt Silence's text names: an interrupt-cast effect for 3 s.
    uint32 const SILENCE_INTERRUPT = 32747;

    /// 15487: Silence: when the holder has been added to a target that is no player (Priest::SilenceInterrupts) and its
    /// caster is known, the caster casts the interrupt (32747) at the target, triggered. Nothing at the call before the
    /// add, for a player and for an apply whose caster cannot be found.
    SpellHandlerOutcome<void> SilenceApplied(AuraApplyContext& ctx)
    {
        if (ctx.real && ctx.caster && Priest::SilenceInterrupts(ctx.target.GetTypeId()))
        {
            ctx.CastTriggeredFrom(*ctx.caster, ctx.target, SILENCE_INTERRUPT);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterSilence(SpellHandlerRegistry& registry)
{
    return RegisterRanks<AuraApplyHook>(registry, SILENCE, &SilenceApplied);
}

bool Priest::SilenceInterrupts(uint8 targetType)
{
    return targetType != TYPEID_PLAYER;
}
