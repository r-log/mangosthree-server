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

    uint32 const REACTIVE_BARRIER_BUFF = 86347;

    /// 86303, 86304: Reactive Barrier: keeps for its proc a hit that an absorb took whole (no damage, some absorbed,
    /// a normal or a critical hit) while its owner is below half his health, a hit the engine's own check refuses as
    /// one that dealt nothing; every other event is left to that check.
    SpellHandlerOutcome<bool> ReactiveBarrierCheckProc(CheckProcContext& ctx)
    {
        if (ctx.damage == 0 && ctx.absorbed && (ctx.procEx & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT)) &&
            Mage::ReactiveBarrierTriggers(ctx.owner.GetHealth(), ctx.owner.GetMaxHealth(), 0))
        {
            return SpellHandlerOutcome<bool>::Return(true);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    /// 86303, 86304: Reactive Barrier: a proc-trigger aura whose own trigger spell (18350) the client's data lacks.
    /// When the hit, its damage after absorbs taken from his health before it, leaves its owner alive and below half
    /// his health (Mage::ReactiveBarrierTriggers) and Ice Barrier (11426) is not on cooldown for him, he casts the free
    /// Ice Barrier buff (86347: Ice Barrier's cost -100%, one charge) on himself, then Ice Barrier, triggered: it costs
    /// nothing, starts no global cooldown and starts its own cooldown and its category's as its cast does. The cast
    /// spends the buff's one charge: the buff is taken off him. FAILED otherwise.
    ProcOutcome ReactiveBarrierProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!Mage::ReactiveBarrierTriggers(ctx.caster.GetHealth(), ctx.caster.GetMaxHealth(), ctx.damage) ||
            ctx.OnCooldown(ICE_BARRIER))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        Mage::CastProcSpell(ctx, ctx.caster, REACTIVE_BARRIER_BUFF);
        SpellAuraProcResult const result = Mage::CastProcSpell(ctx, ctx.caster, ICE_BARRIER);
        ctx.caster.RemoveAurasDueToSpell(REACTIVE_BARRIER_BUFF);
        return ProcOutcome::Return(result);
    }

    uint32 const REACTIVE_BARRIER[] = { 86303, 86304 };
}

bool Mage::ReactiveBarrierTriggers(uint32 health, uint32 maxHealth, uint32 damage)
{
    if (damage >= health)
    {
        return false;
    }
    return uint64(health - damage) * 2 < uint64(maxHealth);
}

namespace Mage
{
    uint32 RegisterReactiveBarrier(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<CheckProcHook>(registry, REACTIVE_BARRIER, &ReactiveBarrierCheckProc);
        rows += RegisterRanks<OnProcHook>(registry, REACTIVE_BARRIER, &ReactiveBarrierProc);
        return rows;
    }
}
