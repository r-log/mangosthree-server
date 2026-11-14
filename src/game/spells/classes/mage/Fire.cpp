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
#include "entities/Unit.h"
#include "Log.h"
#include "Utilities/Util.h"

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

/// one body for 5 labels: 11119, 11120, 12846: Ignite; 12847, 12848: absent from the client's spell data: triggers
/// 12654 for 4 to 20 percent of the damage by rank
static SpellHandlerOutcome<SpellAuraProcResult> DummyAuraProcMage11119(DummyAuraProcContext& ctx)
{
    // Ignite
    switch (ctx.dummySpell->ID)
    {
        case 11119: ctx.basepoints[0] = int32(0.04f * ctx.damage); break;
        case 11120: ctx.basepoints[0] = int32(0.08f * ctx.damage); break;
        case 12846: ctx.basepoints[0] = int32(0.12f * ctx.damage); break;
        case 12847: ctx.basepoints[0] = int32(0.16f * ctx.damage); break;
        case 12848: ctx.basepoints[0] = int32(0.20f * ctx.damage); break;
        default:
            sLog.outError("Unit::HandleDummyAuraProc: non handled spell id: %u (IG)", ctx.dummySpell->ID);
            return SpellHandlerOutcome<SpellAuraProcResult>::Return(SPELL_AURA_PROC_FAILED);
    }

    ctx.triggered_spell_id = 12654;
    return SpellHandlerOutcome<SpellAuraProcResult>::Continue();
}

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

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

    uint32 const HOT_STREAK_BUFF = 48108;
    uint32 const PYROBLAST_INSTANT = 92315;
    uint32 const IMPROVED_HOT_STREAK_RANK_1 = 44446;
    uint32 const IMPROVED_HOT_STREAK_RANK_2 = 44448;

    /// 44445: Hot Streak: each critical strike of Fireball, Frostfire Bolt, Scorch, Pyroblast or Fire Blast (the proc
    /// row's spells; Pyroblast! 92315 is left out) rolls HotStreakChance of the caster's sheet crit for the school of
    /// the spell; when that fails and the caster knows Improved Hot Streak (44448: 100, 44446: 50), two such crits in a
    /// row roll its percent. A non-crit of those spells starts the count again. Success casts Hot Streak (48108) on the
    /// caster. The count lives in this holder's script word of effect 0.
    ProcOutcome HotStreakProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_DUMMY))
        {
            return ProcOutcome::Continue();
        }
        if (ctx.effIndex != EFFECT_INDEX_0)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        if (!ctx.procSpell)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        if (ctx.procSpell->ID == PYROBLAST_INSTANT)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }

        bool const crit = (ctx.procEx & PROC_EX_CRITICAL_HIT) != 0;
        float const sheetCrit = ctx.caster.GetTypeId() == TYPEID_PLAYER
            ? ctx.caster.GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(GetSpellSchoolMask(ctx.procSpell)))
            : 0.0f;
        int32 const improved = ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_2) ? 100
                             : ctx.caster.HasAura(IMPROVED_HOT_STREAK_RANK_1) ? 50 : 0;
        bool const baseRoll = crit && roll_chance_f(Mage::HotStreakChance(sheetCrit));
        bool const improvedRoll = crit && !baseRoll && improved > 0 && roll_chance_i(improved);
        if (!Mage::HotStreakStep(ctx.aura.ScriptWord(EFFECT_INDEX_0), crit, baseRoll, improvedRoll))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        return ProcOutcome::Return(Mage::CastProcSpell(ctx, ctx.caster, HOT_STREAK_BUFF));
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

    uint32 const MASTER_OF_ELEMENTS[] = { 29074, 29075, 29076 };
    uint32 const HOT_STREAK[] = { 44445 };
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

float Mage::HotStreakChance(float sheetCritPercent)
{
    float const crit = sheetCritPercent > 0.0f ? sheetCritPercent : 0.0f;
    float const chance = 95.0f - 2.73f * crit;
    return chance > 0.0f ? chance : 0.0f;
}

bool Mage::HotStreakStep(int32& crits, bool crit, bool baseRollPassed, bool improvedRollPassed)
{
    if (crit && baseRollPassed)
    {
        crits = 0;
        return true;
    }
    return CountCritInARow(crits, crit) && improvedRollPassed;
}

uint32 Mage::RegisterFire(SpellHandlerRegistry& registry)
{
    static Row<AuraBoostMageSite> const boostMage[] =
    {
        { 11129, &AuraBoostMage11129 },
        { 28682, &AuraBoostMage28682 },
    };

    static Row<DummyAuraProcMageSite> const dummyAuraProcMage[] =
    {
        { 11119, &DummyAuraProcMage11119 },
        { 11120, &DummyAuraProcMage11119 },
        { 12846, &DummyAuraProcMage11119 },
        { 12847, &DummyAuraProcMage11119 },
        { 12848, &DummyAuraProcMage11119 },
    };

    uint32 rows = RegisterRows(registry, boostMage);
    rows += RegisterRows(registry, dummyAuraProcMage);
    rows += RegisterRanks<OnProcHook>(registry, MASTER_OF_ELEMENTS, &MasterOfElementsProc);
    rows += RegisterRanks<OnProcHook>(registry, HOT_STREAK, &HotStreakProc);
    rows += RegisterRanks<OnProcHook>(registry, BLAZING_SPEED, &BlazingSpeedProc);
    rows += RegisterRanks<DummyAuraHook>(registry, LIVING_BOMB, &LivingBombDummyAura);
    return rows;
}
