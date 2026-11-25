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
    uint32 const IMPROVED_CONE_OF_COLD_RANK_ONE = 11190;
    uint32 const IMPROVED_CONE_OF_COLD_RANK_TWO = 12489;
    uint32 const IMPROVED_CONE_OF_COLD_FREEZE_ONE = 83301;
    uint32 const IMPROVED_CONE_OF_COLD_FREEZE_TWO = 83302;

    /// 120: Cone of Cold: after its damage, for a caster who knows Improved Cone of Cold (12489, or 11190), the living
    /// unit it hit casts the rank's freeze (Mage::ImprovedConeOfColdFreeze) on itself, for the caster. A unit immune to
    /// the freeze as a spell of the caster's is not frozen: the freeze shows Immune at it instead. Nothing for a heal.
    SpellHandlerOutcome<void> ConeOfColdAfterHit(AfterHitContext& ctx)
    {
        if (ctx.heal || !ctx.target.IsAlive())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = ctx.caster.HasAura(IMPROVED_CONE_OF_COLD_RANK_TWO) ? IMPROVED_CONE_OF_COLD_RANK_TWO :
                              ctx.caster.HasAura(IMPROVED_CONE_OF_COLD_RANK_ONE) ? IMPROVED_CONE_OF_COLD_RANK_ONE : 0;
        uint32 const freeze = Mage::ImprovedConeOfColdFreeze(talent);
        SpellEntry const* freezeSpell = freeze ? sSpellStore.LookupEntry(freeze) : NULL;
        if (!freezeSpell)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (ctx.target.IsImmuneToSpell(freezeSpell, false))
        {
            ctx.ShowImmune(ctx.target, freeze);
            return SpellHandlerOutcome<void>::Continue();
        }
        ctx.CastTriggeredFrom(ctx.target, ctx.target, freeze);
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const CONE_OF_COLD_HIT[] = { 120 };
}

uint32 Mage::ImprovedConeOfColdFreeze(uint32 talentSpell)
{
    switch (talentSpell)
    {
        case IMPROVED_CONE_OF_COLD_RANK_ONE:
            return IMPROVED_CONE_OF_COLD_FREEZE_ONE;
        case IMPROVED_CONE_OF_COLD_RANK_TWO:
            return IMPROVED_CONE_OF_COLD_FREEZE_TWO;
        default:
            return 0;
    }
}

namespace Mage
{
    uint32 RegisterConeOfCold(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AfterHitHook>(registry, CONE_OF_COLD_HIT, &ConeOfColdAfterHit);
    }
}
