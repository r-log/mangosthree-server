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
    /// 12536: Clearcasting: the proc that takes its charge goes on to the other checks only for a spell its cost
    /// modifier makes cheaper (IsCostReducedByModifier: a mage spell of its class mask with a mana cost above 0).
    /// Every other event is refused, so Pyroblast! (92315), the free Flamestrike (88148), Blizzard's damage (42208), a
    /// spell with no class options (an item's use among them) and an event with no spell leave Clearcasting on.
    SpellHandlerOutcome<bool> ClearcastingCheckProc(CheckProcContext& ctx)
    {
        if (!ctx.procSpell || !IsCostReducedByModifier(&ctx.spell, EFFECT_INDEX_0, ctx.procSpell))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }

    uint32 const CLEARCASTING[] = { 12536 };
}

namespace Mage
{
    uint32 RegisterClearcasting(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<CheckProcHook>(registry, CLEARCASTING, &ClearcastingCheckProc);
    }
}
