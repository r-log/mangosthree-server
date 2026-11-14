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

#include "spells/classes/mage/Mage.h"
#include "spells/handlers/AurasHandlers.h"
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "combat/SpellBonus.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>

/// 11129: Combustion
static SpellHandlerOutcome<void> AuraBoostMage11129(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 28682;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

/// 28682: absent from the client's spell data
static SpellHandlerOutcome<void> AuraBoostMage28682(AuraBoostContext& ctx)
{
    if (!ctx.apply)
    {
        ctx.spellId1 = 11129;
    }
    else
    {
        return SpellHandlerOutcome<void>::Return();
    }
    return SpellHandlerOutcome<void>::Continue();
}

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// Ignite's damage over time, the spell the talent's ranks cast.
    uint32 const IGNITE_DOT = 12654;

    /// The Fire hits that do not ignite: Living Bomb's explosion, Flame Orb's hits and Fire Power's explosion.
    uint32 const NOT_IGNITING[] = { 44461, 82739, 83619 };

    /// The ticks Ignite (12654) deals: its duration over its periodic effect's period; 0 when the spell store has no
    /// 12654 or its effect no period.
    uint32 IgniteTicks()
    {
        SpellEntry const* ignite = sSpellStore.LookupEntry(IGNITE_DOT);
        SpellEffectEntry const* effect = ignite ? ignite->GetSpellEffect(EFFECT_INDEX_0) : NULL;
        int32 const duration = GetSpellDuration(ignite);
        if (!effect || !effect->EffectAuraPeriod || duration <= 0)
        {
            return 0;
        }
        return uint32(duration) / effect->EffectAuraPeriod;
    }

    /// 11119, 11120, 12846: Ignite: a critical strike of one of the mage's direct Fire spells puts the aura's percent of
    /// its damage on the victim as Ignite (12654), two ticks over its duration. The explosion of Living Bomb (44461),
    /// Flame Orb's hits (82739) and Fire Power's explosion (83619) do not ignite. A share below one point per tick
    /// banks nothing, so a running Ignite is left as it is.
    ProcOutcome IgniteProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!(ctx.procEx & PROC_EX_CRITICAL_HIT))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell &&
            std::find(std::begin(NOT_IGNITING), std::end(NOT_IGNITING), ctx.procSpell->ID) != std::end(NOT_IGNITING))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (!ctx.victim || !ctx.victim->IsAlive() || ctx.triggerAmount <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const tick = Combat::DotShareTick(float(ctx.damage), uint32(ctx.triggerAmount), IgniteTicks());
        if (tick <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        int32 const basePoints[MAX_EFFECT_INDEX] = { tick, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, *ctx.victim, IGNITE_DOT, basePoints));
    }

    /// 29074, 29075, 29076: Master of Elements: after a spell the proc accepts, gives back the aura's percent of the
    /// spell's base mana cost through 29077.
    ProcOutcome MasterOfElementsProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const cost = ctx.procSpell->GetManaCost() + ctx.procSpell->GetManaCostPercentage() * ctx.caster.GetCreateMana() / 100;
        int32 const basePoints[MAX_EFFECT_INDEX] = { cost * ctx.triggerAmount / 100, 0, 0 };
        if (basePoints[EFFECT_INDEX_0] <= 0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, 29077, basePoints));
    }

    /// 44445: Hot Streak; 44446, 44448: Improved Hot Streak: effect 0 counts the hits the proc accepts in the holder's
    /// script word of effect 0, and the second crit in a row rolls the aura's chance for Hot Streak! (48108). Effect 1
    /// does nothing, and neither does effect 0 while the owner holds no aura of the spell at effect 1.
    ProcOutcome HotStreakProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.caster.GetAura(ctx.spell.ID, EFFECT_INDEX_1))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }

        if (Mage::CountCritInARow(ctx.aura.ScriptWord(EFFECT_INDEX_0), (ctx.procEx & PROC_EX_CRITICAL_HIT) != 0) &&
            roll_chance_i(ctx.triggerAmount))
        {
            ctx.CastTriggered(ctx.caster, 48108);
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }

    /// 31641, 31642: Blazing Speed: a proc-trigger aura whose own trigger spell (18350) the client's data lacks. Its
    /// proc casts Blazing Speed (31643) instead, through Mage::CastProcTriggerSpell with no target of its own; the
    /// aura's amount as base points when the aura carries a value. No row of the legacy site of trigger spells
    /// (ProcTriggerSpellAuraProcTriggeredSite) names 31643.
    ProcOutcome BlazingSpeedProc(OnProcContext& ctx)
    {
        bool const withValue = Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
        if (!withValue && !Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!IsBlazingSpeedRank(ctx.spell.ID))
        {
            sLog.outError("Blazing Speed: spell %u is no rank of Blazing Speed", ctx.spell.ID);
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const basePoints[MAX_EFFECT_INDEX] = { withValue ? ctx.triggerAmount : 0, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, 31643, NULL, basePoints));
    }

    /// 44457: Living Bomb: when its dummy effect is un-applied because the aura ran out or was dispelled, the target
    /// casts the effect's amount, the explosion (44461), at itself, triggered by this aura effect. Every un-apply ends
    /// the dummy aura handler; an apply goes on to its other steps.
    SpellHandlerOutcome<void> LivingBombDummyAura(DummyAuraContext& ctx)
    {
        if (ctx.apply)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (ctx.mode == AURA_REMOVE_BY_EXPIRE || ctx.mode == AURA_REMOVE_BY_DISPEL)
        {
            ctx.CastTriggeredByAura(ctx.target, ctx.target, uint32(ctx.amount));
        }
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const IGNITE[] = { 11119, 11120, 12846 };
    uint32 const MASTER_OF_ELEMENTS[] = { 29074, 29075, 29076 };
    uint32 const HOT_STREAK[] = { 44445, 44446, 44448 };
    uint32 const BLAZING_SPEED[] = { 31641, 31642 };
    uint32 const LIVING_BOMB[] = { 44457 };
}

bool Mage::CountCritInARow(int32& crits, bool crit)
{
    if (!crit)
    {
        crits = 0;
        return false;
    }
    if (++crits < 2)
    {
        return false;
    }
    crits = 0;
    return true;
}

uint32 Mage::RegisterFire(SpellHandlerRegistry& registry)
{
    static Row<AuraBoostMageSite> const boostMage[] =
    {
        { 11129, &AuraBoostMage11129 },
        { 28682, &AuraBoostMage28682 },
    };

    uint32 rows = RegisterRows(registry, boostMage);
    rows += RegisterRanks<OnProcHook>(registry, IGNITE, &IgniteProc);
    rows += RegisterRanks<OnProcHook>(registry, MASTER_OF_ELEMENTS, &MasterOfElementsProc);
    rows += RegisterRanks<OnProcHook>(registry, HOT_STREAK, &HotStreakProc);
    rows += RegisterRanks<OnProcHook>(registry, BLAZING_SPEED, &BlazingSpeedProc);
    rows += RegisterRanks<DummyAuraHook>(registry, LIVING_BOMB, &LivingBombDummyAura);
    return rows;
}
