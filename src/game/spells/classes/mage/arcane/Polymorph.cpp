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
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellScriptQueries.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Log.h"

#include <algorithm>
#include <iterator>

namespace
{
    uint32 const IMPROVED_POLYMORPH_RANK_ONE = 11210;
    uint32 const IMPROVED_POLYMORPH_RANK_TWO = 12592;
    uint32 const IMPROVED_POLYMORPH_STUN_RANK_ONE = 83046;
    uint32 const IMPROVED_POLYMORPH_STUN_RANK_TWO = 83047;
    uint32 const IMPROVED_POLYMORPH_MARKER = 87515;

    /// The aura types of a damage over time effect: periodic damage, a periodic health leech, and periodic damage of a
    /// percent of the target's maximum health.
    AuraType const DAMAGE_OVER_TIME_AURA_TYPES[] =
    {
        SPELL_AURA_PERIODIC_DAMAGE, SPELL_AURA_PERIODIC_LEECH, SPELL_AURA_PERIODIC_DAMAGE_PERCENT
    };

    /// 118, 28271, 28272, 61025, 61305, 61721, 61780: Polymorph, each form a mage casts: when damage broke it and its
    /// caster knows Improved Polymorph, the target casts the rank's stun and Improved Polymorph Marker (87515) at
    /// itself for the Polymorph's caster, unless it holds the marker (Mage::ImprovedPolymorphStun). The removal's
    /// boosts run.
    SpellHandlerOutcome<void> PolymorphRemove(AuraRemoveContext& ctx)
    {
        bool const knowsRankOne = ctx.caster && ctx.caster->HasAura(IMPROVED_POLYMORPH_RANK_ONE);
        bool const knowsRankTwo = ctx.caster && ctx.caster->HasAura(IMPROVED_POLYMORPH_RANK_TWO);
        uint32 const stun = Mage::ImprovedPolymorphStun(ctx.mode, knowsRankOne, knowsRankTwo,
                                                        ctx.target.HasAura(IMPROVED_POLYMORPH_MARKER));
        if (stun)
        {
            ctx.CastTriggered(ctx.target, stun);
            ctx.CastTriggered(ctx.target, IMPROVED_POLYMORPH_MARKER);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const GLYPH_OF_POLYMORPH = 56375;

    /// 118, 28271, 28272, 61025, 61305, 61721, 61780: Polymorph, each form a mage casts: as it lands, a caster who
    /// holds Glyph of Polymorph (56375) takes every damage over time aura off the target, whoever cast it
    /// (Mage::GlyphOfPolymorphRemoves). Nothing at the call before the add, and nothing for a caster without the
    /// glyph. The holder's boosts run.
    SpellHandlerOutcome<void> PolymorphApply(AuraApplyContext& ctx)
    {
        if (!ctx.real)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        bool const holdsGlyph = ctx.caster && ctx.caster->HasAura(GLYPH_OF_POLYMORPH);
        for (AuraType type : DAMAGE_OVER_TIME_AURA_TYPES)
        {
            if (Mage::GlyphOfPolymorphRemoves(type, holdsGlyph))
            {
                ctx.target.RemoveSpellsCausingAura(type);
            }
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const POLYMORPH[] = { 118, 28271, 28272, 61025, 61305, 61721, 61780 };
}

uint32 Mage::ImprovedPolymorphStun(AuraRemoveMode mode, bool knowsRankOne, bool knowsRankTwo, bool targetMarked)
{
    if (mode != AURA_REMOVE_BY_DAMAGE || targetMarked)
    {
        return 0;
    }
    if (knowsRankTwo)
    {
        return IMPROVED_POLYMORPH_STUN_RANK_TWO;
    }
    return knowsRankOne ? IMPROVED_POLYMORPH_STUN_RANK_ONE : 0;
}

bool Mage::GlyphOfPolymorphRemoves(uint32 auraType, bool casterHoldsGlyph)
{
    if (!casterHoldsGlyph)
    {
        return false;
    }
    for (AuraType type : DAMAGE_OVER_TIME_AURA_TYPES)
    {
        if (auraType == uint32(type))
        {
            return true;
        }
    }
    return false;
}

namespace Mage
{
    uint32 RegisterPolymorph(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<AuraRemoveHook>(registry, POLYMORPH, &PolymorphRemove);
        rows += RegisterRanks<AuraApplyHook>(registry, POLYMORPH, &PolymorphApply);
        return rows;
    }
}
