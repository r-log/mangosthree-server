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

#include "spells/classes/priest/shadow/Phantasm.h"
#include "spells/data/SpellMgr.h"
#include "entities/Unit.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

namespace
{
    /// The talent's two ranks, the first rank first. Effect 0 of each is a dummy effect that holds the chance in
    /// percent (50 and 100): "$s1% chance" in the client's text.
    uint32 const PHANTASM_RANK_1 = 47569;
    uint32 const PHANTASM_RANK_2 = 47570;

    /// Fade.
    uint32 const FADE[] = { 586 };

    /// The value of effect 0 of a rank of the talent in the client's data; 0 for any other spell and when the store
    /// lacks it.
    int32 RankValue(uint32 spellId)
    {
        if (spellId != PHANTASM_RANK_1 && spellId != PHANTASM_RANK_2)
        {
            return 0;
        }
        SpellEntry const* rank = sSpellStore.LookupEntry(spellId);
        return rank ? rank->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
    }

    /// 586: Fade: once the holder stands on a priest who holds a rank of Phantasm (not at the call before the add), a
    /// roll at the rank's chance (a chance of 100 draws none) takes every harmful root and snare off him.
    SpellHandlerOutcome<void> FadeApplied(AuraApplyContext& ctx)
    {
        if (!ctx.real)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const chance = Priest::PhantasmChance(Priest::PhantasmRank(ctx.target));
        if (chance > 0 && (chance >= 100 || Priest::PhantasmRolls(chance, irand(0, 99))))
        {
            ctx.target.RemoveAurasAtMechanicImmunity(IMMUNE_TO_ROOT_AND_SNARE_MASK, 0, true);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterPhantasm(SpellHandlerRegistry& registry)
{
    return RegisterRanks<AuraApplyHook>(registry, FADE, &FadeApplied);
}

uint32 Priest::PhantasmRank(Unit const& priest)
{
    if (priest.HasAura(PHANTASM_RANK_2))
    {
        return PHANTASM_RANK_2;
    }
    return priest.HasAura(PHANTASM_RANK_1) ? PHANTASM_RANK_1 : 0;
}

int32 Priest::PhantasmChance(uint32 rank)
{
    return RankValue(rank);
}

bool Priest::PhantasmRolls(int32 chance, int32 roll)
{
    return chance >= 100 || (chance > 0 && roll < chance);
}
