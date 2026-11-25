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

#include "spells/classes/priest/InnerSanctum.h"
#include "entities/Unit.h"
#include "DBCStores.h"

#include <cstddef>

namespace
{
    uint32 const INNER_FIRE = 588;
    uint32 const SPELL_WARDING = 91724;

    /// Inner Sanctum's three ranks, the first first.
    uint32 const INNER_SANCTUM_RANKS[] = { 14747, 14770, 14771 };

    uint32 const INNER_FIRE_SPELL[] = { INNER_FIRE };

    /// The value of effect `eff` of `spellId` in the client's data; 0 when the store lacks the spell or the effect.
    int32 EffectValue(uint32 spellId, SpellEffectIndex eff)
    {
        SpellEntry const* spell = spellId ? sSpellStore.LookupEntry(spellId) : NULL;
        return spell ? spell->CalculateSimpleValue(eff) : 0;
    }

    /// 588: Inner Fire: when its holder is added to the priest, not while he loads, he casts Spell Warding (91724) at
    /// himself, triggered, its effect 0 at the points of the Inner Sanctum rank he holds (Priest::SpellWardingPoints
    /// of the rank's effect 0 value in the client's data: -2, -4, -6), so his spell damage taken falls by the rank's
    /// percent while Inner Fire stands. The points are set at the cast: a rank learned or lost later changes them at
    /// Inner Fire's next cast. A recast first takes the old Inner Fire off, and Spell Warding with it, then casts
    /// Spell Warding anew. Nothing without a rank, and nothing at the call before the stacking checks. A Spell
    /// Warding saved with Inner Fire comes back from the character's auras at login.
    SpellHandlerOutcome<void> InnerFireApplied(AuraApplyContext& ctx)
    {
        if (!ctx.real || ctx.target.IsLoading())
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const points = Priest::SpellWardingPoints(EffectValue(Priest::InnerSanctumRank(ctx.target),
                                                                    EFFECT_INDEX_0));
        if (points)
        {
            int32 const basePoints[MAX_EFFECT_INDEX] = { points, 0, 0 };
            ctx.CastTriggered(ctx.target, SPELL_WARDING, basePoints);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 588: Inner Fire: when it leaves the priest, for any reason but the unit's deletion, Spell Warding (91724)
    /// leaves him with it. The holder's boosts run after it.
    SpellHandlerOutcome<void> InnerFireRemoved(AuraRemoveContext& ctx)
    {
        ctx.RemoveAura(ctx.target, SPELL_WARDING);
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterInnerSanctum(SpellHandlerRegistry& registry)
{
    uint32 rows = RegisterRanks<AuraApplyHook>(registry, INNER_FIRE_SPELL, &InnerFireApplied);
    rows += RegisterRanks<AuraRemoveHook>(registry, INNER_FIRE_SPELL, &InnerFireRemoved);
    return rows;
}

uint32 Priest::InnerSanctumRank(Unit const& priest)
{
    for (std::size_t i = sizeof(INNER_SANCTUM_RANKS) / sizeof(INNER_SANCTUM_RANKS[0]); i > 0; --i)
    {
        if (priest.HasAura(INNER_SANCTUM_RANKS[i - 1]))
        {
            return INNER_SANCTUM_RANKS[i - 1];
        }
    }
    return 0;
}

int32 Priest::SpellWardingPoints(int32 rankValue)
{
    if (rankValue <= 0)
    {
        return 0;
    }
    return rankValue < 100 ? -rankValue : -100;
}
