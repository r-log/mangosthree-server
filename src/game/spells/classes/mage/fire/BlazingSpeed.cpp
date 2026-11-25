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
    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// 31641, 31642: Blazing Speed: a proc-trigger aura whose own trigger spell (18350) the client's data lacks. Its
    /// proc casts Blazing Speed (31643) instead, through Mage::CastProcTriggerSpell with no target of its own; the
    /// aura's amount as base points when the aura carries a value. No row of the legacy site of trigger spells
    /// (ProcTriggerSpellAuraProcTriggeredSite) names 31643.
    ProcOutcome BlazingSpeedProc(OnProcContext& ctx)
    {
        bool const withValue = Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL_WITH_VALUE);
        if (!withValue && !Mage::ProcEffectIs(ctx, SPELL_AURA_PROC_TRIGGER_SPELL))
        {
            return ProcOutcome::Continue();
        }
        if (!IsBlazingSpeedRank(ctx.spell.ID))
        {
            sLog.outError("Blazing Speed: spell %u is no rank of Blazing Speed", ctx.spell.ID);
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }

        int32 const basePoints[MAX_EFFECT_INDEX] = { withValue ? ctx.triggerAmount : 0, 0, 0 };
        return ProcOutcome::Return(Mage::CastProcTriggerSpell(ctx, 31643, NULL, basePoints));
    }

    uint32 const BLAZING_SPEED[] = { 31641, 31642 };
}

namespace Mage
{
    uint32 RegisterBlazingSpeed(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<OnProcHook>(registry, BLAZING_SPEED, &BlazingSpeedProc);
    }
}
