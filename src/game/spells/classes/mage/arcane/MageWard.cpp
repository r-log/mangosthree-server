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
    uint32 const MOLTEN_SHIELDS = 11094;
    uint32 const BLAZING_SPEED_BUFF = 31643;

    /// 543: Mage Ward: when the ward is removed because damage used up its last point and its caster knows Molten
    /// Shields (11094), Blazing Speed (31643) is cast on that caster from the unit the ward was on, for him; the ward
    /// is self-cast, so both are the mage, and his spell modifiers reach the cast. Any other removal casts nothing.
    /// The removal's boosts still run.
    SpellHandlerOutcome<void> MageWardAuraRemove(AuraRemoveContext& ctx)
    {
        if (Mage::MageWardTriggersBlazingSpeed(ctx.mode, ctx.caster && ctx.caster->HasAura(MOLTEN_SHIELDS)))
        {
            ctx.CastTriggered(*ctx.caster, BLAZING_SPEED_BUFF);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const MAGE_WARD[] = { 543 };
}

bool Mage::MageWardTriggersBlazingSpeed(AuraRemoveMode mode, bool casterKnowsMoltenShields)
{
    return mode == AURA_REMOVE_BY_SHIELD_BREAK && casterKnowsMoltenShields;
}

namespace Mage
{
    uint32 RegisterMageWard(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AuraRemoveHook>(registry, MAGE_WARD, &MageWardAuraRemove);
    }
}
