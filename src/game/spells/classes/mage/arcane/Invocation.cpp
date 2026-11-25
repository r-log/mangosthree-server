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
    uint32 const COUNTERSPELL = 2139;

    /// 84722, 84723: Invocation: answers true for a hit of the mage's Counterspell that interrupted a spell its target
    /// was casting (Mage::InvocationGrants), so the engine's proc casts the buff (87098) at the rank's value though the
    /// hit dealt no damage; every other event is dropped.
    SpellHandlerOutcome<bool> InvocationCheckProc(CheckProcContext& ctx)
    {
        uint32 const procSpellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Mage::InvocationGrants(procSpellId, ctx.procEx));
    }

    uint32 const INVOCATION[] = { 84722, 84723 };
}

bool Mage::InvocationGrants(uint32 procSpellId, uint32 procEx)
{
    return procSpellId == COUNTERSPELL && SpellMgr::InterruptedACast(procEx);
}

namespace Mage
{
    uint32 RegisterInvocation(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<CheckProcHook>(registry, INVOCATION, &InvocationCheckProc);
    }
}
