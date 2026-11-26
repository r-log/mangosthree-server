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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 54646: Focus Magic: its proc-trigger effect (1, Focus Magic's buff 54648) casts nothing. Cast by the unit Focus
    /// Magic is on, the buff, whose one effect is at its caster, would land on that unit; the mage's own cast of it at
    /// himself comes from the crit-chance effect's proc (FocusMagicBuffCaster), so effect 0 goes on to it.
    ProcOutcome FocusMagicProc(OnProcContext& ctx)
    {
        if (!Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
    }

    uint32 const FOCUS_MAGIC[] = { 54646 };
}

namespace Mage
{
    uint32 RegisterFocusMagic(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, FOCUS_MAGIC, &FocusMagicProc);
    }
}
