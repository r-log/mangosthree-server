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

#include "spells/LustLockout.h"
#include "spells/SpellScriptHooks.h"
#include "spells/data/SpellTables.h"

namespace
{
    /// Every haste spell of LustLockoutOf's table: of the units the engine found for an effect, drops every one that
    /// holds a lockout (Spells::HoldsLustLockout); the rest keep the haste.
    SpellHandlerOutcome<void> LustSelectTargets(SelectTargetsContext& ctx)
    {
        ctx.targets.remove_if([](Unit const* unit) { return unit && Spells::HoldsLustLockout(*unit); });
        return SpellHandlerOutcome<void>::Continue();
    }

    /// Every haste spell of LustLockoutOf's table: when the haste lands on a unit that holds no lockout, that unit
    /// casts the spell's lockout (LustLockoutOf) on itself, triggered, as its own cast. The call before the holder is
    /// added does nothing, and neither does a landing on a unit that already holds a lockout, as a haste loaded with
    /// its unit's saved auras may land.
    SpellHandlerOutcome<void> LustAuraApply(AuraApplyContext& ctx)
    {
        if (ctx.real && !Spells::HoldsLustLockout(ctx.target))
        {
            ctx.CastTriggeredFrom(ctx.target, ctx.target, LustLockoutOf(ctx.spell.ID));
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

bool Spells::HoldsLustLockout(Unit const& unit)
{
    for (uint32 index = 0; LustSpellAt(index); ++index)
    {
        if (unit.HasAura(LustLockoutOf(LustSpellAt(index))))
        {
            return true;
        }
    }
    return false;
}

uint32 RegisterLustScripts(SpellHandlerRegistry& registry)
{
    uint32 rows = 0;
    for (uint32 index = 0; LustSpellAt(index); ++index)
    {
        RegisterScript<SelectTargetsHook>(registry, LustSpellAt(index), &LustSelectTargets);
        RegisterScript<AuraApplyHook>(registry, LustSpellAt(index), &LustAuraApply);
        rows += 2;
    }
    return rows;
}
