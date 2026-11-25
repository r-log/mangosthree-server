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
#include "spells/handlers/UnitAuraProcHandlerHandlers.h"
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellTables.h"
#include "spells/SpellModMgr.h"
#include "spells/SpellScriptQueries.h"
#include "spells/PeriodicTiming.h"
#include "combat/SpellBonus.h"
#include "combat/ThreatManager.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Log.h"
#include "Utilities/Util.h"

#include <algorithm>
#include <iterator>
#include <vector>

namespace
{
    /// 44457: Living Bomb: when its dummy effect is un-applied because the aura ran out (Mage::LivingBombExplodes),
    /// the target casts the effect's amount, the explosion (44461), at itself, triggered by this aura effect; any
    /// other removal casts nothing. Every un-apply ends the dummy aura handler; an apply goes on to its other steps.
    SpellHandlerOutcome<void> LivingBombDummyAura(DummyAuraContext& ctx)
    {
        if (ctx.apply)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        if (Mage::LivingBombExplodes(ctx.mode))
        {
            ctx.CastTriggeredByAura(ctx.target, ctx.target, uint32(ctx.amount));
        }
        return SpellHandlerOutcome<void>::Return();
    }

    uint32 const LIVING_BOMB[] = { 44457 };
}

bool Mage::LivingBombExplodes(AuraRemoveMode mode)
{
    return mode == AURA_REMOVE_BY_EXPIRE;
}

namespace Mage
{
    uint32 RegisterLivingBomb(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<DummyAuraHook>(registry, LIVING_BOMB, &LivingBombDummyAura);
    }
}
