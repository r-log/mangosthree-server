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

#include "spells/classes/priest/holy/DivineTouch.h"
#include "spells/data/SpellMgr.h"
#include "spells/PeriodicTiming.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstdint>

namespace
{
    /// Divine Touch's ranks: each a dummy aura whose effect 0 holds the rank's percent (5, 10).
    uint32 const DIVINE_TOUCH[] = { 63534, 63542 };

    /// Renew, the heal over time whose hit starts Divine Touch.
    uint32 const RENEW = 139;

    /// Divine Touch's heal, cast at the unit the priest's Renew hit.
    uint32 const DIVINE_TOUCH_HEAL = 63544;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 63534, 63542: Divine Touch: on a hit of the priest's Renew that landed (Priest::DivineTouchFrom), the priest
    /// casts its heal (63544) at the unit hit, triggered by the rank's aura: the rank's percent (the aura's amount,
    /// effect 0's 5 or 10) of the tick his Renew there holds (its amount, his done-side bonus included, before a
    /// crit), times Renew's base duration over its period as his Renew's aura reckons it (Priest::RenewPeriod),
    /// through Priest::DivineTouchHeal. The heal's hit starts procs (TriggeredCastStartsProcs). CANT_TRIGGER for an
    /// effect but effect 0 and for every event that is not such a hit, its own heal's among them; FAILED, with nothing
    /// cast, when the unit holds no Renew of his, the heal comes to 0 or the cast is refused.
    ProcOutcome DivineTouchProc(OnProcContext& ctx)
    {
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.procSpell ||
            !Priest::DivineTouchFrom(ctx.procFlag, ctx.procEx, ctx.procSpell->ID))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        SpellAuraHolder* renew = ctx.victim ? ctx.victim->GetSpellAuraHolder(RENEW, ctx.caster.GetObjectGuid()) : NULL;
        if (!renew)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        AuraHandle const held(*renew);
        int32 const heal = Priest::DivineTouchHeal(held.Amount(EFFECT_INDEX_0), ctx.triggerAmount,
                                                   GetSpellDuration(ctx.procSpell),
                                                   Priest::RenewPeriod(ctx.caster, *ctx.procSpell));
        if (heal <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { heal, 0, 0 };
        return ProcOutcome::Return(ctx.CastTriggered(*ctx.victim, DIVINE_TOUCH_HEAL, basePoints)
                                   ? SPELL_AURA_PROC_OK : SPELL_AURA_PROC_FAILED);
    }
}

uint32 Priest::RegisterDivineTouch(SpellHandlerRegistry& registry)
{
    return RegisterRanks<OnProcHook>(registry, DIVINE_TOUCH, &DivineTouchProc);
}

bool Priest::DivineTouchFrom(uint32 procFlag, uint32 procEx, uint32 spellId)
{
    bool const helpfulHit = (procFlag & PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL) != 0;
    bool const tick = (procFlag & PROC_FLAG_ON_DO_PERIODIC) || (procEx & PROC_EX_PERIODIC_POSITIVE);
    bool const landed = (procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) != 0;
    return spellId == RENEW && helpfulHit && landed && !tick;
}

int32 Priest::RenewPeriod(Unit const& priest, SpellEntry const& renew)
{
    SpellEffectEntry const* effect = renew.GetSpellEffect(EFFECT_INDEX_0);
    if (!effect || !effect->EffectAuraPeriod)
    {
        return 0;
    }
    int32 period = int32(effect->EffectAuraPeriod);
    if (SpellModMgr* modOwner = priest.GetSpellMods())
    {
        modOwner->ApplySpellMod(renew.ID, SPELLMOD_ACTIVATION_TIME, period);
    }
    if (renew.HasAttribute(SPELL_ATTR_EX5_HASTE_AFFECTS_PERIODIC) && !renew.HasAttribute(SPELL_ATTR_EX3_NO_DONE_BONUS))
    {
        period = Spells::HastedPeriod(period, priest.GetFloatValue(UNIT_MOD_CAST_HASTE));
    }
    return period;
}

int32 Priest::DivineTouchHeal(int32 tick, int32 percent, int32 baseDurationMs, int32 periodMs)
{
    if (tick <= 0 || percent <= 0 || baseDurationMs <= 0 || periodMs <= 0)
    {
        return 0;
    }
    double const heal = double(tick) * double(percent) / 100.0 * double(baseDurationMs) / double(periodMs) + 0.5;
    return heal < double(INT32_MAX) ? int32(heal) : INT32_MAX;
}
