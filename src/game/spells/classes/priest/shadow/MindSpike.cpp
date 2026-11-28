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

#include "spells/classes/priest/shadow/MindSpike.h"
#include "spells/classes/priest/shadow/ShadowOrbs.h"
#include "entities/Unit.h"

namespace
{
    uint32 const MIND_BLAST[] = { 8092 };
    uint32 const MIND_SPIKE[] = { 73510 };

    /// Mind Spike's debuff on its target: its aura 308 raises the caster's next Mind Blast's crit chance by 30% a
    /// stack, for three stacks, 12 s.
    uint32 const MIND_SPIKE_DEBUFF = 87178;

    /// The damage over time Mind Spike puts out, and the id of each: Shadow Word: Pain, Devouring Plague, Vampiric
    /// Touch.
    uint32 const EXTINGUISHED[] = { 589, 2944, 34914 };

    /// 8092: Mind Blast: after a hit the orbs are spent (Priest::ConsumeShadowOrbs), then the caster's own Mind Spike
    /// debuff leaves the target, whether this Mind Blast crit or not: the crit chance was rolled before the hit.
    SpellHandlerOutcome<void> MindBlastAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        Priest::ConsumeShadowOrbs(ctx);
        ctx.target.RemoveAurasByCasterSpell(MIND_SPIKE_DEBUFF, ctx.caster.GetObjectGuid());
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 73510: Mind Spike: after a hit the orbs are spent (Priest::ConsumeShadowOrbs), then the caster's own Shadow Word:
    /// Pain, Devouring Plague and Vampiric Touch leave the target (Priest::MindSpikeExtinguishes).
    SpellHandlerOutcome<void> MindSpikeAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        Priest::ConsumeShadowOrbs(ctx);
        for (uint32 spellId : EXTINGUISHED)
        {
            ctx.target.RemoveAurasByCasterSpell(spellId, ctx.caster.GetObjectGuid());
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterMindSpike(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AfterHitHook>(registry, MIND_BLAST, &MindBlastAfterHit);
    rows += RegisterRanks<AfterHitHook>(registry, MIND_SPIKE, &MindSpikeAfterHit);
    return rows;
}

bool Priest::MindSpikeExtinguishes(uint32 spellId)
{
    for (uint32 extinguished : EXTINGUISHED)
    {
        if (spellId == extinguished)
        {
            return true;
        }
    }
    return false;
}
