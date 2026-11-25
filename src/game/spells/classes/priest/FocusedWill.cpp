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

#include "spells/classes/priest/FocusedWill.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

namespace
{
    /// The talent's first rank, whose effect 1 holds the percent of his maximum health a hit must pass (10); the client
    /// text of both ranks reads it there.
    uint32 const FOCUSED_WILL_RANK_1 = 45234;

    /// The talent's two ranks.
    uint32 const FOCUSED_WILL_RANKS[] = { FOCUSED_WILL_RANK_1, 45243 };

    /// The percent of his maximum health a hit's damage must pass: the first rank's effect 1 value in the client's
    /// data; 0 when the store lacks the spell.
    int32 FocusedWillPercent()
    {
        SpellEntry const* rank = sSpellStore.LookupEntry(FOCUSED_WILL_RANK_1);
        return rank ? rank->CalculateSimpleValue(EFFECT_INDEX_1) : 0;
    }

    /// 45234, 45243: Focused Will: its spell_proc_event row lets the normal and the critical hits he takes through;
    /// the script refuses a normal hit whose damage is not greater than the first rank's percent of his maximum health
    /// (Priest::FocusedWillTakes). A critical hit and a larger hit reach the proc-trigger aura, which gives him its
    /// rank's Focused Will (45241, 45242).
    SpellHandlerOutcome<bool> FocusedWillCheckProc(CheckProcContext& ctx)
    {
        if (!Priest::FocusedWillTakes(ctx.procEx, ctx.damage, ctx.owner.GetMaxHealth(), FocusedWillPercent()))
        {
            return SpellHandlerOutcome<bool>::Return(false);
        }
        return SpellHandlerOutcome<bool>::Continue();
    }
}

uint32 Priest::RegisterFocusedWill(SpellHandlerRegistry& registry)
{
    return RegisterRanks<CheckProcHook>(registry, FOCUSED_WILL_RANKS, &FocusedWillCheckProc);
}

bool Priest::FocusedWillTakes(uint32 procEx, uint32 damage, uint32 maxHealth, int32 percent)
{
    if (procEx & PROC_EX_CRITICAL_HIT)
    {
        return true;
    }
    if (!(procEx & PROC_EX_NORMAL_HIT) || percent <= 0)
    {
        return false;
    }
    return uint64(damage) * 100 > uint64(maxHealth) * uint64(percent);
}
