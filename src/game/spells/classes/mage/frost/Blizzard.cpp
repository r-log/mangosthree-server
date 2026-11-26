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
    uint32 const ICE_SHARDS_RANK_ONE = 11185;
    uint32 const ICE_SHARDS_RANK_TWO = 12487;
    uint32 const ICE_SHARDS_CHILL_ONE = 12484;
    uint32 const ICE_SHARDS_CHILL_TWO = 12485;

    /// 42208: Blizzard's damage: after it, for a caster who knows Ice Shards (12487, or 11185), the caster casts the
    /// rank's chill (Mage::IceShardsChill) at the living unit it hit, as the talent's own aura casts it: the chill's
    /// hit and its cast end start the procs of the auras that can proc from procs alone (TriggeredCastStartsProcsOf).
    /// Nothing for a heal.
    SpellHandlerOutcome<void> BlizzardAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal || !ctx.target.IsAlive())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = ctx.caster.HasAura(ICE_SHARDS_RANK_TWO) ? ICE_SHARDS_RANK_TWO :
                              ctx.caster.HasAura(ICE_SHARDS_RANK_ONE) ? ICE_SHARDS_RANK_ONE : 0;
        uint32 const chill = Mage::IceShardsChill(talent);
        if (chill)
        {
            ctx.CastTriggeredByAura(ctx.target, chill, talent, EFFECT_INDEX_0);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const BLIZZARD_HIT[] = { 42208 };
}

uint32 Mage::IceShardsChill(uint32 talentSpell)
{
    switch (talentSpell)
    {
        case ICE_SHARDS_RANK_ONE:
            return ICE_SHARDS_CHILL_ONE;
        case ICE_SHARDS_RANK_TWO:
            return ICE_SHARDS_CHILL_TWO;
        default:
            return 0;
    }
}

namespace Mage
{
    uint32 RegisterBlizzard(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AfterHitHook>(registry, BLIZZARD_HIT, &BlizzardAfterHit);
    }
}
