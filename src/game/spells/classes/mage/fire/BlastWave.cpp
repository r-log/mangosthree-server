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
    uint32 const IMPROVED_FLAMESTRIKE_RANK_1 = 84673;
    uint32 const IMPROVED_FLAMESTRIKE_RANK_2 = 84674;
    uint32 const FREE_FLAMESTRIKE = 88148;

    /// 11113: Blast Wave: when the cast hit two or more units the caster is not friendly to (the context's count) and
    /// the caster knows Improved Flamestrike (84674, or 84673), the rank's chance (ImprovedFlamestrikeChance) is rolled
    /// and on success the caster casts the free Flamestrike (88148) at the Blast Wave's point, triggered with no
    /// triggering aura. A cast with neither a destination nor a unit target has no point of its own and calls nothing.
    SpellHandlerOutcome<void> BlastWaveAfterCast(AfterCastContext& ctx)
    {
        if (ctx.enemiesHit < 2 || (!ctx.hasDestination && !ctx.target))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        uint32 const talent = ctx.caster.HasAura(IMPROVED_FLAMESTRIKE_RANK_2) ? IMPROVED_FLAMESTRIKE_RANK_2
                            : ctx.caster.HasAura(IMPROVED_FLAMESTRIKE_RANK_1) ? IMPROVED_FLAMESTRIKE_RANK_1 : 0;
        int32 const chance = Mage::ImprovedFlamestrikeChance(talent);
        if (chance <= 0 || !roll_chance_i(chance))
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        ctx.CastTriggeredAt(ctx.destX, ctx.destY, ctx.destZ, FREE_FLAMESTRIKE);
        return SpellHandlerOutcome<void>::Continue();
    }

    uint32 const BLAST_WAVE[] = { 11113 };
    uint32 const IMPROVED_FLAMESTRIKE[] = { IMPROVED_FLAMESTRIKE_RANK_1, IMPROVED_FLAMESTRIKE_RANK_2 };
}

int32 Mage::ImprovedFlamestrikeChance(uint32 talentSpell)
{
    uint32 const* const ranksEnd = std::end(IMPROVED_FLAMESTRIKE);
    if (std::find(std::begin(IMPROVED_FLAMESTRIKE), ranksEnd, talentSpell) == ranksEnd)
    {
        return 0;
    }
    SpellEntry const* talent = sSpellStore.LookupEntry(talentSpell);
    return talent ? talent->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
}

namespace Mage
{
    uint32 RegisterBlastWave(SpellHandlerRegistry& registry)
    {
        return RegisterRanks<AfterCastHook>(registry, BLAST_WAVE, &BlastWaveAfterCast);
    }
}
