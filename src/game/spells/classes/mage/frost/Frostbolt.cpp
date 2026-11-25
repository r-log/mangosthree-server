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
    uint32 const EARLY_FROST_RANK_ONE = 83049;
    uint32 const EARLY_FROST_RANK_TWO = 83050;
    uint32 const EARLY_FROST_LOCKOUT_RANK_ONE = 83162;
    uint32 const EARLY_FROST_LOCKOUT_RANK_TWO = 83239;
    uint32 const PIERCING_CHILL_CHILL = 83154;

    /// 116: as a Frostbolt cast begins, the Early Frost lockout of the rank its caster knows (Mage::EarlyFrostLockout)
    /// is cast on him; while it lasts, its modifiers cancel the talent's on his next Frostbolts' cast time and global
    /// cooldown. This cast's own are already set, and a cast that is interrupted keeps the lockout it started.
    SpellHandlerOutcome<void> FrostboltCastStart(CastStartContext& ctx)
    {
        uint32 const lockout = Mage::EarlyFrostLockout(ctx.caster.HasAura(EARLY_FROST_RANK_ONE),
                                                       ctx.caster.HasAura(EARLY_FROST_RANK_TWO),
                                                       ctx.caster.HasAura(EARLY_FROST_LOCKOUT_RANK_ONE) ||
                                                       ctx.caster.HasAura(EARLY_FROST_LOCKOUT_RANK_TWO));
        if (lockout)
        {
            ctx.CastTriggered(ctx.caster, lockout);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 116: Frostbolt: after a hit that was a critical strike, for a caster who knows Piercing Chill (83157, or 83156)
    /// with a count above 0 (Mage::PiercingChillCount), the chill (83154) is cast once, at the target, through the
    /// talent's aura, a target the crit killed included; the chill's own target pick (PiercingChillTargets) chooses
    /// the units it chills. Nothing for a heal or a hit that was no crit.
    SpellHandlerOutcome<void> FrostboltAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal || !ctx.crit)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = PiercingChillTalent(ctx.caster);
        if (Mage::PiercingChillCount(talent) <= 0)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        ctx.CastTriggeredByAura(ctx.target, PIERCING_CHILL_CHILL, talent, EFFECT_INDEX_0);
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const FROSTBOLT_CAST_START[] = { FROSTBOLT };
    uint32 const FROSTBOLT_HIT[] = { 116 };
}

uint32 Mage::EarlyFrostLockout(bool knowsRankOne, bool knowsRankTwo, bool lockoutUp)
{
    if (lockoutUp)
    {
        return 0;
    }
    return knowsRankTwo ? EARLY_FROST_LOCKOUT_RANK_TWO : knowsRankOne ? EARLY_FROST_LOCKOUT_RANK_ONE : 0;
}

namespace Mage
{
    uint32 RegisterFrostbolt(SpellHandlerRegistry& registry)
    {
        uint32 rows = RegisterRanks<CastStartHook>(registry, FROSTBOLT_CAST_START, &FrostboltCastStart);
        rows += RegisterRanks<AfterHitHook>(registry, FROSTBOLT_HIT, &FrostboltAfterHit);
        return rows;
    }
}
