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
    uint32 const SHATTERED_BARRIER_RANK_ONE = 44745;
    uint32 const SHATTERED_BARRIER_RANK_TWO = 54787;
    uint32 const SHATTERED_BARRIER_FREEZE_RANK_ONE = 55080;
    uint32 const SHATTERED_BARRIER_FREEZE_RANK_TWO = 83073;
    uint32 const GLYPH_OF_ICE_BARRIER = 63095;

    /// 11426: Ice Barrier: at the holder's add, for a caster who knows Glyph of Ice Barrier (63095), the shield's
    /// amount, its base and its spell power part together, is raised by the glyph's effect 0 value in percent
    /// (Mage::GlyphedBarrierAmount). A shield loaded with its owner keeps the amount it was saved with. The holder's
    /// boosts still run.
    SpellHandlerOutcome<void> IceBarrierAuraApply(AuraApplyContext& ctx)
    {
        if (ctx.real && !ctx.target.IsLoading() && ctx.caster && ctx.caster->HasAura(GLYPH_OF_ICE_BARRIER))
        {
            int32 const percent = EffectValue(GLYPH_OF_ICE_BARRIER, EFFECT_INDEX_0);
            ctx.aura.SetAmount(EFFECT_INDEX_0, Mage::GlyphedBarrierAmount(ctx.aura.Amount(EFFECT_INDEX_0), percent));
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 11426: Ice Barrier: when damage used up its last point and its caster knows Shattered Barrier, the caster casts
    /// the freeze of the rank he knows (Mage::ShatteredBarrierFreeze) at himself: a root on every enemy within the
    /// freeze's radius of him. A barrier that runs out, is dispelled, cancelled or replaced casts nothing. The
    /// removal's boosts still run.
    SpellHandlerOutcome<void> IceBarrierAuraRemove(AuraRemoveContext& ctx)
    {
        if (!ctx.caster)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const freeze = Mage::ShatteredBarrierFreeze(ctx.mode, ctx.caster->HasAura(SHATTERED_BARRIER_RANK_ONE),
                                                           ctx.caster->HasAura(SHATTERED_BARRIER_RANK_TWO));
        if (freeze)
        {
            ctx.CastTriggered(*ctx.caster, freeze);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const ICE_BARRIER_IDS[] = { ICE_BARRIER };
}

uint32 Mage::ShatteredBarrierFreeze(AuraRemoveMode mode, bool knowsRankOne, bool knowsRankTwo)
{
    if (mode != AURA_REMOVE_BY_SHIELD_BREAK)
    {
        return 0;
    }
    return knowsRankTwo ? SHATTERED_BARRIER_FREEZE_RANK_TWO : knowsRankOne ? SHATTERED_BARRIER_FREEZE_RANK_ONE : 0;
}

int32 Mage::GlyphedBarrierAmount(int32 amount, int32 percent)
{
    return percent > 0 ? amount + amount * percent / 100 : amount;
}

namespace Mage
{
    uint32 RegisterIceBarrier(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<AuraApplyHook>(registry, ICE_BARRIER_IDS, &IceBarrierAuraApply);
        rows += RegisterRanks<AuraRemoveHook>(registry, ICE_BARRIER_IDS, &IceBarrierAuraRemove);
        return rows;
    }
}
