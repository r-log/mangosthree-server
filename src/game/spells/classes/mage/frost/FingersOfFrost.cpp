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
#include "spells/classes/mage/frost/FrostShared.h"
#include "spells/pipeline/Spell.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellModMgr.h"
#include "entities/Unit.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

namespace
{
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    uint32 const FINGERS_OF_FROST_BUFF = 44544;
    uint32 const DEEP_FREEZE = 44572;
    uint32 const FROST_ARMOR_CHILL = 7321;
    uint32 const FROSTFIRE_ORB_RANK_ONE_HIT = 95969;

    /// 30455, 44572, 44614, 133: as the cast goes off, before its cast-end proc, takes the Fingers of Frost charge it
    /// uses (UsesFingers), then takes Brain Freeze's buff off the caster when the buff made the cast instant
    /// (UsesBrainFreeze), whatever then becomes of the cast: one that misses, meets an immunity or finds its target
    /// dead takes both all the same.
    SpellHandlerOutcome<void> FrostLaunch(LaunchContext& ctx)
    {
        if (Mage::UsesFingers(ctx.caster, ctx.spell, ctx.castTime))
        {
            Mage::UseFingersCharge(ctx.caster);
        }
        if (Mage::UsesBrainFreeze(ctx.caster, ctx.spell, ctx.castTime))
        {
            ctx.caster.RemoveAurasDueToSpell(BRAIN_FREEZE_BUFF);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 44543, 44545, 83074 (Fingers of Frost), whose row takes the hit of each chill it names, and 44546, 44548, 44549
    /// (Brain Freeze), whose row takes the end of the cast: each talent refuses the procs of the sources
    /// RefusesChillSource names.
    SpellHandlerOutcome<bool> ChillSourceCheckProc(CheckProcContext& ctx)
    {
        if (ctx.procSpell && Mage::RefusesChillSource(ctx.procSpell->ID))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 44543, 44545, 83074: Fingers of Frost: its proc rolls when a chill of its row lands, once for each unit the
    /// spell hits or crits, and never at the end of a cast nor at a miss or an immunity (the row's hit kinds decide);
    /// it rolls the aura's amount as a percent chance, and on a pass casts its trigger spell (44544) through
    /// Mage::CastProcTriggerSpell.
    ProcOutcome FingersOfFrostProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!roll_chance_i(ctx.triggerAmount))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, Mage::EffectTriggerSpell(ctx), NULL));
    }

    uint32 const FINGERS_OF_FROST[] = { 44543, 44545, 83074 };
    uint32 const BRAIN_FREEZE[] = { 44546, 44548, 44549 };
    uint32 const FROST_LAUNCHES[] = { 30455, 44572, 44614, 133 };
}

bool Mage::UsesFingers(Unit const& caster, SpellEntry const& spell, uint32 castTime)
{
    if (!caster.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    return spell.ID == ICE_LANCE_SPELL || spell.ID == DEEP_FREEZE ||
           (spell.ID == FROSTFIRE_BOLT && UsesBrainFreeze(caster, spell, castTime));
}

bool Mage::RefusesChillSource(uint32 procSpellId)
{
    return procSpellId == FROST_ARMOR_CHILL || procSpellId == FROSTFIRE_ORB_RANK_ONE_HIT;
}

bool Mage::UseFingersCharge(Unit& mage)
{
    if (!mage.HasAura(FINGERS_OF_FROST_BUFF))
    {
        return false;
    }
    mage.RemoveAuraHolderFromStack(FINGERS_OF_FROST_BUFF);
    return true;
}

void Mage::GiveFingers(Unit& mage, uint32 charges)
{
    for (uint32 i = 0; i < charges; ++i)
    {
        mage.CastSpell(&mage, FINGERS_OF_FROST_BUFF, true);
    }
}

float Mage::IceLanceMultiplier(bool countsAsFrozen, bool fingersUp, int32 doubledPercent, int32 fingersPercent)
{
    float const frozen = countsAsFrozen ? (100.0f + doubledPercent) / 100.0f : 1.0f;
    return fingersUp ? frozen * (100.0f + fingersPercent) / 100.0f : frozen;
}

namespace Mage
{
    uint32 RegisterFingersOfFrost(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<OnProcHook>(registry, FINGERS_OF_FROST, &FingersOfFrostProc);
        rows += RegisterRanks<CheckProcHook>(registry, FINGERS_OF_FROST, &ChillSourceCheckProc);
        rows += RegisterRanks<CheckProcHook>(registry, BRAIN_FREEZE, &ChillSourceCheckProc);
        rows += RegisterRanks<LaunchHook>(registry, FROST_LAUNCHES, &FrostLaunch);
        return rows;
    }
}
