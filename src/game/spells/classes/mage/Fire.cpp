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

    uint32 const MASTER_OF_ELEMENTS[] = { 29074, 29075, 29076 };
    uint32 const HOT_STREAK[] = { 44445, 44446, 44448 };
    uint32 const BLAZING_SPEED[] = { 31641, 31642 };
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
    return rows;
}
