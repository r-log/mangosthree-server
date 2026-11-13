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
#include "entities/Unit.h"
#include "Log.h"

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

    uint32 const MASTER_OF_ELEMENTS[] = { 29074, 29075, 29076 };
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
    return rows;
}
