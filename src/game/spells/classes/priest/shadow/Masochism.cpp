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

#include "spells/classes/priest/shadow/Masochism.h"
#include "spells/classes/priest/shadow/ShadowWordDeath.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"

namespace
{
    /// The talent's first rank, whose effect 1 holds the percent of his maximum health a hit must reach (10); the
    /// client text of both ranks reads it there.
    uint32 const MASOCHISM_RANK_1 = 88994;

    /// The talent's two ranks.
    uint32 const MASOCHISM_RANKS[] = { MASOCHISM_RANK_1, 88995 };

    /// The percent of his maximum health a hit's damage must reach: the first rank's effect 1 value in the client's
    /// data; 0 when the store lacks the spell.
    int32 MasochismPercent()
    {
        SpellEntry const* rank = sSpellStore.LookupEntry(MASOCHISM_RANK_1);
        return rank ? rank->CalculateSimpleValue(EFFECT_INDEX_1) : 0;
    }

    /// 88994, 88995: Masochism: its proc flags take every hit he takes and his periodic damage, and it has no
    /// spell_proc_event row, so the script's answer is the whole check: true for an event Priest::MasochismTakes
    /// accepts, false for every other, which no longer reaches the aura's trigger. An answer of true skips the class
    /// mask of the engine's check, which Shadow Word: Death's recoil does not fit.
    SpellHandlerOutcome<bool> MasochismCheckProc(CheckProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Priest::MasochismTakes(spellId, ctx.procFlag, ctx.damage,
                                                                        ctx.owner.GetMaxHealth(), MasochismPercent()));
    }
}

uint32 Priest::RegisterMasochism(SpellHandlerRegistry& registry)
{
    return RegisterRanks<CheckProcHook>(registry, MASOCHISM_RANKS, &MasochismCheckProc);
}

bool Priest::MasochismTakes(uint32 spellId, uint32 procFlag, uint32 damage, uint32 maxHealth, int32 percent)
{
    if (procFlag & PROC_FLAG_ON_TAKE_PERIODIC)
    {
        return spellId == SHADOW_WORD_DEATH_RECOIL;
    }
    return percent > 0 && maxHealth && uint64(damage) * 100 >= uint64(maxHealth) * uint32(percent);
}
