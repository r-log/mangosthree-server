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

#include "spells/classes/priest/shadow/ImprovedDevouringPlague.h"
#include "entities/Unit.h"

#include <cstdint>

namespace
{
    /// Improved Devouring Plague's two ranks: each a dummy aura whose effect 0 holds the rank's percent (15, 30) of the
    /// plague's periodic effect its hit deals.
    uint32 const IMPROVED_DEVOURING_PLAGUE[] = { 63625, 63626 };

    /// Devouring Plague, the periodic damage whose total the hit takes a share of.
    uint32 const DEVOURING_PLAGUE = 2944;

    /// The talent's hit: a damage effect of no coefficient, cast with the share as its amount.
    uint32 const IMPROVED_DEVOURING_PLAGUE_HIT = 63675;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 63625, 63626: Improved Devouring Plague: on a hit of the priest's Devouring Plague, the priest casts the talent's
    /// hit (63675) at the unit hit, triggered by the rank's aura, for the rank's percent (the aura's amount, effect 0's
    /// 15 or 30) of what his plague there will deal in all: the tick it holds (its amount, his done bonus included)
    /// times the ticks its time left and its hasted period give (Priest::DevouringPlagueTicks), through
    /// Priest::ImprovedDevouringPlagueHit. The hit takes no done bonus of his and starts procs (63675 is in the
    /// proc-starting table). CANT_TRIGGER for an effect but effect 0; FAILED, with nothing cast, when the event has no
    /// spell or no unit, the unit holds no plague of his, the hit comes to 0 or the cast is refused.
    ProcOutcome ImprovedDevouringPlagueProc(OnProcContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        SpellAuraHolder* plague = ctx.victim && ctx.procSpell
            ? ctx.victim->GetSpellAuraHolder(DEVOURING_PLAGUE, ctx.caster.GetObjectGuid()) : NULL;
        if (!plague)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        AuraHandle const held(*plague);
        int32 const ticks = Priest::DevouringPlagueTicks(held.Duration(), held.Period(EFFECT_INDEX_0));
        int32 const hit = Priest::ImprovedDevouringPlagueHit(held.Amount(EFFECT_INDEX_0), ticks, ctx.triggerAmount);
        if (hit <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { hit, 0, 0 };
        return ProcOutcome::Return(ctx.CastTriggered(*ctx.victim, IMPROVED_DEVOURING_PLAGUE_HIT, basePoints)
                                   ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED);
    }
}

uint32 Priest::RegisterImprovedDevouringPlague(SpellHandlerRegistry& registry)
{
    return RegisterRanks<OnProcHook>(registry, IMPROVED_DEVOURING_PLAGUE, &ImprovedDevouringPlagueProc);
}

int32 Priest::DevouringPlagueTicks(int32 remainingMs, int32 periodMs)
{
    if (remainingMs <= 0 || periodMs <= 0)
    {
        return 0;
    }
    return int32((int64(remainingMs) + int64(periodMs) - 1) / int64(periodMs));
}

int32 Priest::ImprovedDevouringPlagueHit(int32 tick, int32 ticks, int32 percent)
{
    if (tick <= 0 || ticks <= 0 || percent <= 0)
    {
        return 0;
    }
    int64 const hit = int64(tick) * int64(ticks) * int64(percent) / 100;
    return hit < int64(INT32_MAX) ? int32(hit) : INT32_MAX;
}
